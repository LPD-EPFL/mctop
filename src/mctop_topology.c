#include <mctop.h>
#include <darray.h>

cdf_cluster_t* mctopo_infer_clustering(uint64_t** lat_table_norm, const size_t N);

mctopo_t* mctopo_create(uint n_sockets, cdf_cluster_t* cc, uint n_hwcs, const int is_smt);
hwc_group_t* mctop_hwc_group_create(mctopo_t* t, uint n_hwcs, darray_t* ids, uint id, uint lvl, uint lat, const int has_smt);
socket_t* mctop_socket_create(mctopo_t* topo, uint n_hwcs, darray_t* hwc_ids, uint seq_id, 
			      uint lvl, uint lat, uint64_t** mem_lat_table, const int is_smt);
void mctop_siblings_create(mctopo_t* topo, uint socket_x_id, uint socket_y_id, uint* seq_id, uint lvl, uint latency);
void mctopo_fix_children_links(mctopo_t* topo);
void mctopo_fix_horizontal_links(mctopo_t* topo);
void mctopo_fix_n_hwcs_per_core_smt(mctopo_t* topo);
void mctopo_mem_latencies_add(mctopo_t* topo, uint64_t** mem_lat_table);

extern void cdf_cluster_free(cdf_cluster_t* cc);
extern cdf_cluster_t* cdf_cluster_create_empty(const int n_clusters);

mctopo_t*
mctopo_construct(uint64_t** lat_table_norm, 
		 const size_t N,
		 uint64_t** mem_lat_table,
		 const uint n_sockets,
		 cdf_cluster_t* cc,
		 const int is_smt)
{
  int free_cc = 0;
  if (cc == NULL)
    {
      cc = mctopo_infer_clustering(lat_table_norm, N);
      free_cc = 1;
    }

  const uint hwc_per_socket = N / n_sockets;
  uint n_sockets_found = 0;
  mctopo_t* topo = mctopo_create(n_sockets, cc, N, is_smt);

  uint8_t* processed = malloc_assert(N * sizeof(uint8_t));
  darray_t* group = darray_create();
  int lvl;
  for (lvl = 0; lvl < cc->n_clusters; lvl++)
    {
      if (n_sockets_found == n_sockets)
	{
	  break;
	}

      uint seq_id = 0;
      uint64_t target_lat = cc->clusters[lvl].median;
      for (int i = 0; i < N; i++)
	{
	  processed[i] = 0;
	}

      size_t n_groups = 0;
      for (int x = 0; x < N; x++)
	{
	  if (processed[x])
	    {
	      continue;
	    }

	  n_groups++;
	  darray_add(group, x);
	  processed[x] = 1;
	  for (int y = x + 1; y < N; y++)
	    {
	      int belongs = 1;
	      for (int w = 0; belongs && w < N; w++)
	      	{
	      	  if (w != y) /* w is not y that is being checked */
	      	    {
		      if (darray_exists(group, w)) /* if w already in group => y must either  */
			{ /* belong with y due to a smaller latency or share the latency as y */
			  belongs = (lat_table_norm[w][y] <= target_lat);
			}
		      else	/* otherwise if w is in larger lat with x and y, then both x */
			{	/* and y should have the same distance to w */
			  belongs = (lat_table_norm[x][w] < target_lat ||
				     lat_table_norm[y][w] < target_lat ||
				     lat_table_norm[x][w] == lat_table_norm[y][w]);
			}
		    }
	      	}

	      if (belongs)
		{
		  darray_add(group, y);
		  processed[y] = 1;
		}
	    }
	  size_t group_size = darray_get_num_elems(group);
	  if (group_size > 1 && group_size < hwc_per_socket) /* within socket */
	    {
	      mctop_hwc_group_create(topo, group_size, group, seq_id, lvl, target_lat, is_smt);
	    }
	  else if (n_sockets_found < n_sockets && group_size == hwc_per_socket) /* socket */
	    {
	      n_sockets_found++;
	      topo->socket_level = lvl;
	      mctop_socket_create(topo, group_size, group, seq_id, lvl, target_lat, mem_lat_table, is_smt);
	    }

	  seq_id++;
	  darray_empty(group);
	}
    }

  darray_free(group);
  free(processed);

  if (lvl < cc->n_clusters)
    {
      uint64_t** lat_table_socket = (uint64_t**) table_calloc(n_sockets, n_sockets, sizeof(uint64_t));
      for (int x = 0; x < n_sockets; x++)
	{
	  socket_t* sx = mctop_get_socket(topo, x);
	  uint hwc_x = mctop_get_first_hwc_socket(sx)->id;
	  for (int y = x + 1; y < n_sockets; y++)
	    {
	      socket_t* sy = mctop_get_socket(topo, y);
	      uint hwc_y = mctop_get_first_hwc_socket(sy)->id;
	      lat_table_socket[x][y] = lat_table_socket[y][x] = lat_table_norm[hwc_x][hwc_y];
	    }
	}


      for (; lvl < cc->n_clusters; lvl++)
	{
	  uint seq_id = 0;
	  uint64_t target_lat = cc->clusters[lvl].median;
	  for (int x = 0; x < n_sockets; x++)
	    {
	      for (int y = x + 1; y < n_sockets; y++)
		{
		  if (lat_table_socket[x][y] == target_lat)
		    {
		      mctop_siblings_create(topo, x, y, &seq_id, lvl, target_lat);
		    }
		}
	    }
	}
      table_free((void**) lat_table_socket, n_sockets);
    }

  if (n_sockets_found != n_sockets)
    {
      fprintf(stderr, "MCTOP Warning: Found %u sockets instead of %u that was expected.\n"
	      "            The generated topology is probably INCORRECT!.\n", n_sockets_found, n_sockets);
    }

  mctopo_fix_children_links(topo);
  mctopo_fix_horizontal_links(topo);
  mctopo_fix_n_hwcs_per_core_smt(topo);
  mctopo_mem_latencies_add(topo, mem_lat_table);

  if (free_cc)
    {
      cdf_cluster_free(cc);
    }

  return topo;
}

void
mctopo_free(mctopo_t* topo)
{
  /* free siblings */
  for (int l = topo->socket_level + 1; l < topo->n_levels; l++)
    {
      sibling_t* sibling = mctop_get_first_sibling_lvl(topo, l);
      do
	{
	  sibling_t* next = sibling->next;
	  free(sibling);
	  sibling = next;
	}
      while (sibling != NULL);
    }

  /* free groups */
  for (int l = 1; l < topo->socket_level; l++)
    {
      hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, l);
      do
	{
	  hwc_gs_t* next = gs->next;
	  free(gs->hwcs);
	  free(gs->children);
	  free(gs);
	  gs = next;
	}
      while (gs != NULL);
    }

  /* free socket stuff */
  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = topo->sockets + s;
      free(socket->hwcs);
      free(socket->children);
      if (socket->n_siblings > 0)
	{
	  free(socket->siblings);
	}
      if (topo->has_mem >= LATENCY)
	{
	  free(socket->mem_latencies);
	}
      if (topo->has_mem == BANDWIDTH)
	{
	  free(socket->mem_bandwidths);
	}
    }

  /* free topo */
  free(topo->latencies);
  free(topo->sockets);
  free(topo->hwcs);
  if (topo->n_siblings > 0)
    {
      free(topo->siblings);
    }
  if (topo->has_mem)
    {
      free(topo->node_to_socket);
    }
  free(topo);
  topo = NULL;
}

void
mctopo_print(mctopo_t* topo)
{
#define PD_0 "|||||||||"
#define PD_1 "||||||"
#define PD_2 "|||"
  printf(PD_0" MCTOP Topology   / #HW contexts: %u / #Sockets: %u / Socket ref.: %u-xxxx / SMT: %d \n", 
	 topo->n_hwcs, topo->n_sockets, topo->socket_level, topo->is_smt);
  printf(PD_0" #Latency lvls: %u / Latencies: ", topo->n_levels);
  for (int i = 0; i < topo->n_levels; i++)
    {
      printf("%-u ", topo->latencies[i]);
    }
  printf("\n");

  /* hwc level */
  int l = 0;
  printf(PD_1" Level %u / Latency: %-4u / Ref level: %u / Type: %s\n",
	 l, topo->latencies[l], l, mctop_get_type_desc(topo->hwcs[0].type));
  printf(PD_2" Hardware contexts: \n");
  printf(PD_2" ");
  for (int i = 0; i < topo->n_hwcs; i++)
    {
      printf("%-4u ", topo->hwcs[i].id);
      if ((i != (topo->n_hwcs - 1)) && ((i + 1) & 15) == 0)
	{
	  printf("\n"PD_2" ");
	}
    }
  printf("\n");

  for (int l = 1; l <= topo->socket_level; l++)
    {
      hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, l);
      printf(PD_1" Level %u / Latency: %-4u / Ref level: %u / Type: %s\n",
	     l, topo->latencies[l], l - 1, mctop_get_type_desc(gs->type));
      printf(PD_2" ID       Members\n");
      while (gs != NULL)
	{
	  printf(PD_2" " MCTOP_ID_PRINTER "   ", MCTOP_ID_PRINT(gs->id));
	  for (int i = 0; i < gs->n_children; i++)
	    {
	      printf(MCTOP_ID_PRINTER "  ", MCTOP_ID_PRINT(gs->children[i]->id));
	      if ((i != (gs->n_children - 1)) && ((i + 1) % 10) == 0)
		{
		  printf("\n"PD_2"          ");
		}
	    }
	  printf("\n");
	  gs = gs->next;
	}

      /* mem. latencies */
      if (topo->has_mem && l == topo->socket_level)
	{
	  printf(PD_2"          Memory latencies (cycles)\n");
	  hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, l);
	  while (gs != NULL)
	    {
	      printf(PD_2" " MCTOP_ID_PRINTER "   ", MCTOP_ID_PRINT(gs->id));
	      for (int n = 0; n < gs->n_nodes; n++)
		{
		  printf("%6u%s ", gs->mem_latencies[n], (gs->local_node == n) ? "*" : " ");
		}
	      printf("\n");
	      gs = gs->next;
	    }
	}

      /* mem. bandwidht */
      if (topo->has_mem == BANDWIDTH && l == topo->socket_level)
	{
	  printf(PD_2"          Memory bandwidths (GB/s)\n");
	  hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, l);
	  while (gs != NULL)
	    {
	      printf(PD_2" " MCTOP_ID_PRINTER "   ", MCTOP_ID_PRINT(gs->id));
	      for (int n = 0; n < gs->n_nodes; n++)
		{
		  printf("%6.2f%s ", gs->mem_bandwidths[n], (gs->local_node == n) ? "*" : " ");
		}
	      printf("\n");
	      gs = gs->next;
	    }
	}
    }

  /* siblings */
  for (int l = topo->socket_level + 1; l < topo->n_levels; l++)
    {
      printf(PD_1" Level %u / Latency: %-4u / Ref level: %u / Type: %s\n",
	     l, topo->latencies[l], topo->socket_level, mctop_get_type_desc(CROSS_SOCKET));
      printf(PD_2" ID       Members\n");
      sibling_t* sibling = mctop_get_first_sibling_lvl(topo, l);
      while (sibling != NULL)
	{
	  printf(PD_2" "MCTOP_ID_PRINTER"   "MCTOP_ID_PRINTER"  "MCTOP_ID_PRINTER"\n", 
		 MCTOP_ID_PRINT(sibling->id), MCTOP_ID_PRINT(sibling->left->id), MCTOP_ID_PRINT(sibling->right->id));
	  sibling = sibling->next;
	}
    }
}


cdf_cluster_t*
mctopo_infer_clustering(uint64_t** lat_table_norm, const size_t N)
{
  darray_t* clusters = darray_create();
  for (int x = 0; x < N; x++)
    {
      for (int y = 0; y < N; y++)
	{
	  darray_add_uniq(clusters, lat_table_norm[x][y]);
	}
    }

  int n_clusters = darray_get_num_elems(clusters);
  darray_sort(clusters);
  printf("## Detected %2d clusters: ", n_clusters);
  darray_print(clusters);

  cdf_cluster_t* cc = cdf_cluster_create_empty(n_clusters);
  cc->n_clusters = n_clusters;
  for (int c = 0; c < n_clusters; c++)
    {
      cc->clusters[c].idx = c;
      cc->clusters[c].median = darray_get_elem_n(clusters, c);
    }

  darray_free(clusters);
  return cc;
}


/* ******************************************************************************** */
/* auxilliary */
/* ******************************************************************************** */

mctopo_t*
mctopo_create(uint n_sockets, cdf_cluster_t* cc, uint n_hwcs, const int is_smt)
{
  mctopo_t* topo = calloc_assert(1, sizeof(mctopo_t));
  topo->is_smt = is_smt;
  topo->has_mem = NO_MEMORY;

  topo->n_sockets = n_sockets;
  topo->sockets = calloc_assert(topo->n_sockets, sizeof(socket_t));

  topo->n_levels = cc->n_clusters;
  topo->latencies = malloc_assert(topo->n_levels * sizeof(uint));
  for (int i = 0; i < topo->n_levels; i++)
    {
      topo->latencies[i] = cc->clusters[i].median;
    }

  topo->n_hwcs = n_hwcs;
  topo->hwcs = calloc_assert(topo->n_hwcs, sizeof(hw_context_t));
  for (int i = 0; i < topo->n_hwcs; i++)
    {
      topo->hwcs[i].id = i;
      topo->hwcs[i].phy_id = i;
      topo->hwcs[i].level = 0;      
      topo->hwcs[i].type = is_smt ? HW_CONTEXT : CORE;
    }
  
  return topo;
}


static inline hw_context_t*
mctop_get_hwc_n(mctopo_t* topo, uint id)
{
  return (topo->hwcs + id);
}

hwc_group_t*
mctop_hwc_group_create(mctopo_t* topo, uint n_hwcs, darray_t* hwc_ids, uint seq_id, uint lvl, uint lat, const int has_smt)
{
  hwc_group_t* group = calloc_assert(1, sizeof(hwc_group_t));
  group->id = mctop_create_id(seq_id, lvl);
  group->level = lvl;
  group->type = (has_smt && lvl == 1) ? CORE : HWC_GROUP;
  group->latency = lat;
  group->n_hwcs = n_hwcs;
  group->hwcs = (hw_context_t**) malloc_assert(group->n_hwcs * sizeof(hw_context_t*));
  group->topo = topo;

  darray_iter_t iter;
  darray_iter_init(&iter, hwc_ids);
  size_t elem, i = 0;
  while (darray_iter_next(&iter, &elem))
    {
      hw_context_t* hwc = mctop_get_hwc_n(topo, elem);
      group->hwcs[i++] = hwc;
      if (hwc->parent == NULL)
	{
	  hwc->parent = group;
	}
      else
	{
	  hwc_group_t* cur = hwc->parent;
	  while (cur->parent != NULL)
	    {
	      cur = cur->parent;
	    }
	  if (cur != group)
	    {
	      cur->parent = group;
	    }
	}
    }

  return group;
}


socket_t*
mctop_socket_create(mctopo_t* topo, 
		    uint n_hwcs, 
		    darray_t* hwc_ids, 
		    uint seq_id,
		    uint lvl, 
		    uint latency, 
		    uint64_t** mem_lat_table, 
		    const int is_smt)
{
  socket_t* socket = topo->sockets + seq_id;
  socket->id = mctop_create_id(seq_id, lvl);
  socket->level = lvl;
  socket->type = SOCKET;
  socket->latency = latency;
  socket->n_hwcs = n_hwcs;
  socket->hwcs = (hw_context_t**) malloc_assert(socket->n_hwcs * sizeof(hw_context_t*));
  socket->n_nodes = topo->n_sockets;
  socket->topo = topo;

  /* uint hwc_seq_id = seq_id * socket->n_hwcs; */

  darray_iter_t iter;
  darray_iter_init(&iter, hwc_ids);

  darray_t* pgroups = darray_create();
  size_t elem, i = 0;
  while (darray_iter_next(&iter, &elem))
    {
      hw_context_t* hwc = mctop_get_hwc_n(topo, elem);
      /* hwc->id = hwc_seq_id++; */
      socket->hwcs[i++] = hwc;
      hwc->socket = socket;

      hwc_group_t* cur = hwc->parent;
      while (cur && cur->parent != NULL && cur->parent != socket)
	{
	  cur = cur->parent;
	}
      if (cur != NULL)
	{
	  cur->parent = socket;
	  darray_add_uniq(pgroups, (uintptr_t) cur);
	}
      else
	{
	  hwc->parent = socket;
	}
    }
  darray_free(pgroups);


  return socket;
}

sibling_t*
mctop_sibling_create(uint seq_id, uint lvl, uint latency, socket_t* left, socket_t* right)
{
  sibling_t* sibling = calloc_assert(1, sizeof(sibling_t));
  sibling->id = mctop_create_id(seq_id, lvl);
  sibling->level = lvl;
  sibling->latency = latency;
  sibling->left = left;
  sibling->right = right;
  return sibling;
}

void
mctop_siblings_create(mctopo_t* topo, uint socket_x_id, uint socket_y_id, uint* seq_id, uint lvl, uint latency)
{
  socket_t* socket_x = mctop_get_socket(topo, socket_x_id);
  socket_t* socket_y = mctop_get_socket(topo, socket_y_id);

  socket_x->siblings = realloc_assert(socket_x->siblings, (socket_x->n_siblings + 1) * sizeof(sibling_t*));
  socket_y->siblings = realloc_assert(socket_y->siblings, (socket_y->n_siblings + 1) * sizeof(sibling_t*));

  sibling_t* sibling = mctop_sibling_create((*seq_id)++, lvl, latency, socket_x, socket_y);
  socket_x->siblings[socket_x->n_siblings++] = sibling;
  socket_y->siblings[socket_y->n_siblings++] = sibling;
}


UNUSED static void
mctopo_socket_fix_socket_refs(socket_t* socket)
{
  for (int i = 0; i < socket->n_hwcs; i++)
    {
      hwc_gs_t* cur = (hwc_gs_t*) socket->hwcs[i];
      while (cur != NULL && cur->type != SOCKET)
	{
	  cur->socket = socket;
	  cur = cur->parent;
	}
    }
}

void
mctopo_fix_children_links(mctopo_t* topo)
{
  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = topo->sockets + s;
      darray_t* children = darray_create();
      darray_t* parents = darray_create();
      darray_t* contents = darray_create();
      for (int h = 0; h < socket->n_hwcs; h++)
      	{
      	  hw_context_t* hwc = socket->hwcs[h];
      	  darray_add_uniq(children, (uintptr_t) hwc);
      	}

      for (int lvl = 0; lvl < topo->socket_level; lvl++)
	{
	  /* get parents */
	  DARRAY_FOR_EACH(children, i)
	    {
	      hwc_gs_t* gs = (hwc_gs_t*) DARRAY_GET_N(children, i);
	      gs->socket = socket;
	      darray_add_uniq(parents, (uintptr_t) gs->parent);
	    }
	  DARRAY_FOR_EACH(parents, p)
	    {
	      hwc_gs_t* gsp = (hwc_gs_t*) DARRAY_GET_N(parents, p);
	      DARRAY_FOR_EACH(children, c)
		{
		  hwc_gs_t* gsc = (hwc_gs_t*) DARRAY_GET_N(children, c);
		  gsc->socket = socket;
		  if (gsc->parent == gsp)
		    {
		      darray_add(contents, (uintptr_t) gsc);
		    }
		}

	      gsp->n_children = darray_get_num_elems(contents);
	      gsp->children = malloc_assert(gsp->n_children * sizeof(hwc_gs_t*));
	      DARRAY_FOR_EACH(contents, c)
		{
		  gsp->children[c] = (hwc_gs_t*) DARRAY_GET_N(contents, c);
		}

	      darray_empty(contents);
	    }

	  darray_copy(children, parents);
	  darray_empty(parents);
	}

      darray_free(children);
      darray_free(parents);
    }
}

void
mctopo_fix_horizontal_links(mctopo_t* topo)
{
  darray_t* smt_hwcs = darray_create();
  hw_context_t* hwc_prev_socket = NULL;
  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = topo->sockets + s;
      hw_context_t* hwc_cur = socket->hwcs[0];
      if (hwc_prev_socket != NULL)
	{
	  hwc_prev_socket->next = hwc_cur;
	}
      int r;
      for (r = 1;  r < socket->n_hwcs; r++)
	{
	  if (mctop_are_hwcs_same_core(hwc_cur, socket->hwcs[r]))
	    {
	      darray_add(smt_hwcs, (uintptr_t) socket->hwcs[r]);
	    }
	  else
	    {
	      hwc_cur->next = socket->hwcs[r];
	      hwc_cur = hwc_cur->next;
	    }
	}

      DARRAY_FOR_EACH(smt_hwcs, i)
	{
	  hw_context_t* hwc = (hw_context_t*) DARRAY_GET_N(smt_hwcs, i);
	  hwc_cur->next = hwc;
	  hwc_cur = hwc_cur->next;
	}
      hwc_prev_socket = hwc_cur;

      darray_empty(smt_hwcs);
    }

  darray_t* hwgs = smt_hwcs;	// just a synonym
  hw_context_t* hwc_cur = topo->hwcs;
  while (hwc_cur != NULL)
    {
      darray_add_uniq(hwgs, (uintptr_t) hwc_cur->parent);
      hwc_cur = hwc_cur->next;
    }

  int do_next_lvl = 0;

  do
    {
      DARRAY_FOR_EACH_FROM(hwgs, i, 1)
	{
	  hwc_gs_t* gsp = (hwc_gs_t*) DARRAY_GET_N(hwgs, i - 1);
	  hwc_gs_t* gsc = (hwc_gs_t*) DARRAY_GET_N(hwgs, i);
	  gsp->next = gsc;
	}
      hwc_gs_t* first = (hwc_gs_t*) darray_get_elem_n(hwgs, 0);
  
      do_next_lvl = 0;
      if (first->parent != NULL)
	{
	  do_next_lvl = 1;
	  darray_empty(hwgs);
	  while (first != NULL)
	    {
	      darray_add_uniq(hwgs, (uintptr_t) first->parent);
	      first = first->next;
	    }
	}
    }
  while (do_next_lvl);

  /* siblings */
  darray_t* siblings_all = darray_create();
  darray_t* siblings = smt_hwcs; // just a synonym
  for (int l = topo->socket_level + 1; l < topo->n_levels; l++)
    {
      darray_empty(siblings);
      for (int s = 0; s < topo->n_sockets; s++)
	{
	  socket_t* socket = topo->sockets + s;
	  for (int i = 0; i < socket->n_siblings; i++)
	    {
	      sibling_t* sibling = socket->siblings[i];
	      if (sibling->level == l)
		{
		  darray_add_uniq(siblings_all, (uintptr_t) sibling);
		  darray_add_uniq(siblings, (uintptr_t) sibling);
		}
	    }
	}
      DARRAY_FOR_EACH_FROM(siblings, i, 1)
	{
	  sibling_t* left = (sibling_t*) DARRAY_GET_N(siblings, i - 1);
	  sibling_t* right = (sibling_t*) DARRAY_GET_N(siblings, i);
	  left->next = right;
	}
    }

  topo->n_siblings = darray_get_num_elems(siblings_all);
  topo->siblings = malloc_assert(topo->n_siblings * sizeof(sibling_t*));
  DARRAY_FOR_EACH(siblings_all, i)
    {
      sibling_t* sibling =  (sibling_t*) DARRAY_GET_N(siblings_all, i);
      topo->siblings[i] = sibling;
    }

  darray_free(smt_hwcs);
  darray_free(siblings_all);
}


void
mctopo_mem_latencies_add(mctopo_t* topo, uint64_t** mem_lat_table)
{
  /* mem. latencies */
  if (mem_lat_table != NULL)
    {
      topo->has_mem = topo->has_mem | LATENCY;
      topo->node_to_socket = calloc_assert(topo->n_sockets, sizeof(uint));

      for (int s = 0; s < topo->n_sockets; s++)
	{
	  socket_t* socket = &topo->sockets[s];
	  uint hwc_use_id = 0;
	  uint min_avg = -1;
	  for (int i = 0; i < socket->n_hwcs; i++)
	    {
	      uint hwc_id = socket->hwcs[i]->id;
	      uint avg = 0;
	      for (int s = 0; s < topo->n_sockets; s++)
		{
		  avg += mem_lat_table[hwc_id][s];
		}
	      avg /= topo->n_sockets;
	      if (avg > 0 && avg < min_avg)
		{
		  min_avg = avg;
		  hwc_use_id = hwc_id;
		}
	    }

	  uint64_t* lats = mem_lat_table[hwc_use_id];
	  socket->mem_latencies = malloc_assert(topo->n_sockets * sizeof(uint));
	  uint min_lat = -1, local_node = 0;
	  for (int n = 0; n < topo->n_sockets; n++)
	    {
	      if (lats[n] < min_lat)
		{
		  local_node = n;
		  min_lat = lats[n];
		}
	      socket->mem_latencies[n] = lats[n];
	    }
	  socket->local_node = local_node;
	  topo->node_to_socket[socket->local_node] = s;
	}
    }
}


void
mctopo_fix_n_hwcs_per_core_smt(mctopo_t* topo)
{
  if (topo->is_smt)
    {
      hwc_group_t* gs = mctop_get_first_gs_at_lvl(topo, 1);
      assert(gs->type == CORE);
      topo->n_hwcs_per_core = gs->n_children;
    }
}

void 
mctopo_mem_bandwidth_add(mctopo_t* topo, double** mem_bw_table)
{
  topo->has_mem = BANDWIDTH;
  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = topo->sockets + s;
      socket->mem_bandwidths = malloc_assert(socket->n_nodes * sizeof(double));
      for (int n = 0; n < socket->n_nodes; n++)
	{
	  socket->mem_bandwidths[n] = mem_bw_table[s][n];
	}
    }
}
