#include <mctop.h>
#include <darray.h>

cdf_cluster_t* mctopo_infer_clustering(uint64_t** lat_table_norm, const size_t N);
mctopo_t* mctopo_create(uint n_sockets, cdf_cluster_t* cc, uint n_hwcs, const int is_smt);
hwc_group_t* mctop_hwc_group_create(mctopo_t* t, uint n_hwcs, darray_t* ids, uint id, uint lvl, uint lat, const int has_smt);
socket_t* mctop_socket_create(mctopo_t* topo, uint n_hwcs, darray_t* hwc_ids, uint seq_id, uint lvl, uint lat, const int is_smt);
void mctop_siblings_create(mctopo_t* topo, darray_t* hwc_ids, uint* seq_id, uint lvl, uint latency);


mctopo_t*
mctopo_construct(uint64_t** lat_table_norm, const size_t N, cdf_cluster_t* cc, const int is_smt)
{
  int free_cc = 0;
  if (cc == NULL)
    {
      cc = mctopo_infer_clustering(lat_table_norm, N);
      free_cc = 1;
    }

  const uint n_sockets = 4;
  const uint hwc_per_socket = N / n_sockets;
  mctopo_t* topo = mctopo_create(n_sockets, cc, N, is_smt);

  uint8_t* processed = malloc_assert(N * sizeof(uint8_t));
  darray_t* group = darray_create();
  for (int lvl = 1; lvl < cc->n_clusters; lvl++)
    {
      uint seq_id = 0;
      uint64_t target_lat = cc->clusters[lvl].median;
      printf("---- processing lvl %d (%zu)\n", lvl, target_lat);
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
		  /* w != x &&  */
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
	  printf("#%zu - ", n_groups);
	  darray_print(group);
	  
	  size_t group_size = darray_get_num_elems(group);
	  if (group_size < hwc_per_socket)
	    {
	      /* printf("** within socket lvl = %u\n", lvl); */
	      mctop_hwc_group_create(topo, group_size, group, seq_id, lvl, target_lat, is_smt);
	    }
	  else if (group_size == hwc_per_socket)
	    {
	      /* printf("** socket lvl = %u\n", lvl); */
	      mctop_socket_create(topo, group_size, group, seq_id, lvl, target_lat, is_smt);
	    }
	  else
	    {
	      /* printf("** accross socket lvl = %u\n", lvl); */
	      mctop_siblings_create(topo, group, &seq_id, lvl, target_lat);
	    }

	  seq_id++;
	  darray_empty(group);
	}
    }
  darray_free(group);
  free(processed);
  if (free_cc)
    {
      cdf_cluster_free(cc);
    }

  return topo;
}

void
mctopo_print(mctopo_t* topo)
{
  printf("|||| Topo: %u hwc / %u sockets / %u levels / SMT: %d\n", 
	 topo->n_hwcs, topo->n_sockets, topo->n_levels, topo->is_smt);
  printf("|||||| Latencies: ");
  for (int i = 0; i < topo->n_levels; i++)
    {
      printf("%u ", topo->latencies[i]);
    }
  printf("\n");
  for (int i = 0; i < topo->n_hwcs; i++)
    {
      hw_context_t* hwc = topo->hwcs + i;
      printf(" ctw %-2u -> ", hwc->id);
      hwc_group_t* g = hwc->parent;
      while (g != NULL)
	{
	  printf(" %-2u (l: %u) -> ", g->id, g->latency);
	  g = g->parent;
	}
      socket_t* socket = hwc->socket;
      printf(" %-2u (l: %u) -> (", socket->id, socket->latency);
      for (int s = 0; s < socket->n_siblings; s++)
	{
	  sibling_t* sibling = socket->siblings[s];
	  socket_t* to = sibling->to;
	  printf("%u (l: %u) ", to->id, sibling->latency);
	}
      printf(")\n");
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
      topo->hwcs[i].lvl = 0;      
      topo->hwcs[i].phy_id = i;
      topo->hwcs[i].type = is_smt ? HW_CONTEXT : CORE;
    }
  
  return topo;
}

static inline uint
mctop_create_id(uint seq_id, uint lvl)
{
  return ((lvl * MCTOP_LVL_ID_MULTI) + seq_id);
}

static inline uint
mctop_id_no_lvl(uint id)
{
  return (id % MCTOP_LVL_ID_MULTI);
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
  group->lvl = lvl;
  group->type = (has_smt && lvl == 1) ? CORE : HWC_GROUP;
  group->latency = lat;
  group->n_hwcs = n_hwcs;
  group->hwcs = (hw_context_t**) malloc_assert(group->n_hwcs * sizeof(hw_context_t*));
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
	  cur->parent = group;
	}
    }

  return group;
}

socket_t*
mctop_socket_create(mctopo_t* topo, uint n_hwcs, darray_t* hwc_ids, uint seq_id, uint lvl, uint latency, const int is_smt)
{
  socket_t* socket = topo->sockets + seq_id;
  socket->id = mctop_create_id(seq_id, lvl);
  socket->lvl = lvl;
  socket->type = SOCKET;
  socket->latency = latency;
  socket->is_smt = is_smt;
  socket->n_hwcs = n_hwcs;
  socket->hwcs = (hw_context_t**) malloc_assert(socket->n_hwcs * sizeof(hw_context_t*));
  darray_iter_t iter;
  darray_iter_init(&iter, hwc_ids);

  darray_t* pgroups = darray_create();
  size_t elem, i = 0;
  while (darray_iter_next(&iter, &elem))
    {
      hw_context_t* hwc = mctop_get_hwc_n(topo, elem);
      socket->hwcs[i++] = hwc;
      hwc->socket = socket;

      hwc_group_t* cur = hwc->parent;
      while (cur && cur->parent != NULL && cur->parent->type != SOCKET)
	{
	  cur->socket = socket;
	  cur = cur->parent;
	}
      if (cur != NULL)
	{
	  cur->parent = cur->socket = socket;
	  darray_add_uniq(pgroups, (uintptr_t) cur);
	}
    }

  printf("Socket %u groups: ", socket->id);
  for (int i = 0; i < pgroups->n_elems; i++)
    {
      hwc_group_t* cur = (hwc_group_t*) pgroups->array[i];
      printf("%u ", cur->id);
    }
  printf("\n");

  darray_free(pgroups);
  return socket;
}

sibling_t*
mctop_sibling_create(uint seq_id, uint lvl, uint latency, socket_t* from, socket_t* to)
{
  sibling_t* sibling = calloc_assert(1, sizeof(sibling_t));
  sibling->id = mctop_create_id(seq_id, lvl);
  sibling->lvl = lvl;
  sibling->latency = latency;
  sibling->from = from;
  sibling->to = to;
  return sibling;
}

void
mctop_siblings_create(mctopo_t* topo, darray_t* hwc_ids, uint* seq_id, uint lvl, uint latency)
{
  darray_t* sockets = darray_create();
  darray_iter_t iter;
  darray_iter_init(&iter, hwc_ids);
  size_t elem;
  while (darray_iter_next(&iter, &elem))
    {
      hw_context_t* hwc = mctop_get_hwc_n(topo, elem);
      socket_t* socket = hwc->socket;
      assert(socket != NULL);
      darray_add_uniq(sockets, (uintptr_t) socket);
    }

  uint n_sockets = darray_get_num_elems(sockets);
  uint n_siblings_new = n_sockets - 1;

  for (int i = 0; i < n_sockets; i++)
    {
      socket_t* socket = (socket_t*) darray_get_elem_n(sockets, i);
      socket->siblings = realloc_assert(socket->siblings, n_siblings_new * sizeof(sibling_t*));
    }

  for (int i = 0; i < n_sockets; i++)
    {
      socket_t* socket1 = (socket_t*) darray_get_elem_n(sockets, i);
      for (int j = i + 1; j < n_sockets; j++)
	{
	  socket_t* socket2 = (socket_t*) darray_get_elem_n(sockets, j);

	  printf("# Add siblings %2u : %u <-> %u \n", *seq_id, socket1->id, socket2->id);
	  sibling_t* left = mctop_sibling_create((*seq_id)++, lvl, latency, socket1, socket2);
	  socket1->siblings[socket1->n_siblings++] = left;
	  
	  sibling_t* right = mctop_sibling_create((*seq_id)++, lvl, latency, socket2, socket1);
	  socket2->siblings[socket2->n_siblings++] = right;
	}
    }

  darray_free(sockets);
}
