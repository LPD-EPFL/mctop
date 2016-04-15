#include <mctop_alloc.h>
#include <mctop_internal.h>
#include <darray.h>

static int
floor_log_2(uint n)
{
  int pos = 0;
  if (n >= 1<<16) { n >>= 16; pos += 16; }
  if (n >= 1<< 8) { n >>=  8; pos +=  8; }
  if (n >= 1<< 4) { n >>=  4; pos +=  4; }
  if (n >= 1<< 2) { n >>=  2; pos +=  2; }
  if (n >= 1<< 1) {           pos +=  1; }
  return ((n == 0) ? (-1) : pos);
}

mctop_node_tree_t*
mctop_node_tree_alloc(const uint n_lvls)
{
  mctop_node_tree_t* nt = malloc_assert(sizeof(mctop_node_tree_t));
  nt->levels = malloc_assert(n_lvls * sizeof(mctop_nt_lvl_t));
  nt->n_levels = n_lvls;
  nt->n_nodes = 2 << n_lvls;

  uint n_nodes = 1;
  for (int l = 0; l < n_lvls; l++)
    {
      const uint n_pairs = n_nodes;
      n_nodes <<= 1;
      nt->levels[l].n_pairs = n_pairs;
      nt->levels[l].n_nodes = n_nodes;
      nt->levels[l].pairs = calloc_assert(n_pairs, sizeof(mctop_nt_pair_t));
    }

  return nt;
}

void
mctop_node_tree_free(mctop_node_tree_t* nt)
{
  for (uint l = 0; l < nt->n_levels; l++)
    {
      mctop_nt_lvl_t* level = &nt->levels[l];
      for (uint p = 0; p < level->n_pairs; p++)
	{
	  mctop_nt_pair_t* pair = &level->pairs[p];
	  free(pair->help_nodes);
	  free(pair->help_node_id_offsets);
	}
      free(nt->levels[l].pairs);
      mctop_barrier_destroy(nt->levels[l].barrier);
    }
    free(nt->levels);
    mctop_barrier_destroy(nt->barrier);
    free(nt->barrier);
    free(nt->scratchpad);
    free(nt);
    }

static void
mctop_nt_get_nodes_lvl(mctop_node_tree_t* nt, const uint lvl, darray_t* nodes)
{
  darray_empty(nodes);
  mctop_nt_lvl_t* level = &nt->levels[lvl];
  for (int p = 0; p < level->n_pairs; p++)
    {
      darray_add(nodes, level->pairs[p].nodes[0]);
      darray_add(nodes, level->pairs[p].nodes[1]);
    }
}


static void
mctop_node_tree_add_barriers(mctop_node_tree_t* nt, mctop_type_t barrier_for)
{
  nt->barrier = malloc_assert(sizeof(mctop_barrier_t));
  mctop_barrier_init(nt->barrier, nt->alloc->n_hwcs);

      darray_t* nodes = darray_create();
      for (int l = 0; l < nt->n_levels; l++)
	{
	  mctop_nt_get_nodes_lvl(nt, l, nodes);
	  size_t n_wait_lvl = 0;
	  DARRAY_FOR_EACH(nodes, n)
	    {
	      uint node = DARRAY_GET_N(nodes, n);
	      if (barrier_for == HW_CONTEXT)
		{
		  n_wait_lvl += nt->alloc->n_hwcs_per_socket[node];
		}
	      else if (barrier_for == CORE)
		{
		  n_wait_lvl += nt->alloc->n_cores_per_socket[node];
		}
	    }
      
	  nt->levels[l].barrier = malloc_assert(sizeof(mctop_barrier_t));
	  if (barrier_for == EVERYONE_HWC)
	    {
	      n_wait_lvl = nt->alloc->n_hwcs;
	    }
	  else if (barrier_for == EVERYONE_CORE)
	    {
	      n_wait_lvl = nt->alloc->n_cores;
	    }
	  mctop_barrier_init(nt->levels[l].barrier, n_wait_lvl);
	  /* printf(" LVL %d : %zu threads\n", l, n_wait_lvl); */
	}

      darray_free(nodes);
}


static size_t
mctop_nt_get_n_participants(mctop_node_tree_t* nt, const uint sid, mctop_type_t barrier_for)
{
  size_t n;
  switch (barrier_for)
    {
    case CORE:
    case EVERYONE_CORE:
      n = nt->alloc->n_cores_per_socket[sid];
      break;
    default:
      n = nt->alloc->n_hwcs_per_socket[sid];
      break;
    }
  return n;
}


static void
mctop_node_tree_add_id_offsets(mctop_node_tree_t* nt, mctop_type_t barrier_for)
{
  for (uint l = 0; l < nt->n_levels; l++)
    {
      mctop_nt_lvl_t* level = &nt->levels[l];
      for (uint p = 0; p < level->n_pairs; p++)
	{
	  uint n_hwcs = 0;
	  mctop_nt_pair_t* pair = &level->pairs[p];
	  pair->node_id_offsets[0] = 0;
	  n_hwcs += mctop_nt_get_n_participants(nt, pair->nodes[0], barrier_for);
	  pair->node_id_offsets[1] = n_hwcs;
	  pair->help_node_id_offsets = malloc_assert(pair->n_help_nodes * sizeof(uint));
	  n_hwcs += mctop_nt_get_n_participants(nt, pair->nodes[1], barrier_for);
	  const uint n_hwcs_main = n_hwcs;
	  for (uint h = 0; h < pair->n_help_nodes; h++)
	    {
	      pair->help_node_id_offsets[h] = n_hwcs;
	      n_hwcs += mctop_nt_get_n_participants(nt, pair->help_nodes[h], barrier_for);
	    }

	  if (barrier_for == EVERYONE_HWC || barrier_for == EVERYONE_CORE)
	    {
	      pair->n_hwcs = n_hwcs;
	    }
	  else
	    {
	      pair->n_hwcs = n_hwcs_main;
	    }
	}
    }
}

static mctop_nt_pair_t*
mctop_nt_get_pair(mctop_node_tree_t* nt, const uint lvl, const uint sid)
{
  return &nt->levels[lvl].pairs[sid];
}

static mctop_nt_lvl_t*
mctop_nt_get_level(mctop_node_tree_t* nt, const uint lvl)
{
  return &nt->levels[lvl];
}

static uint 
mctop_nt_get_nth_socket(mctop_node_tree_t* nt, const uint lvl, const uint nth)
{
  return nt->levels[lvl].pairs[nth / 2].socket_ids[nth & 1];
}

void
mctop_node_tree_print(mctop_node_tree_t* nt)
{
  for (int l = 0; l < nt->n_levels; l++)
    {
      mctop_nt_lvl_t* level = nt->levels + l;
      printf("Lvl %d:\t", l);
      for (int p = 0; p < level->n_pairs; p++)
	{
	  mctop_nt_pair_t* pair = &level->pairs[p];
	  socket_t* s0 = mctop_alloc_get_nth_socket(nt->alloc, pair->nodes[0]);
	  socket_t* s1 = mctop_alloc_get_nth_socket(nt->alloc, pair->nodes[1]);
	  double bw = mctop_socket_get_bw_to(s0, s1);
	  uint lat = mctop_ids_get_latency(nt->alloc->topo, s0->id, s1->id);
	    
	  printf("%u[%u] <--[%-3u cy / %-4.1f GB/s]-- %u[%u]   <--  ", s0->id, pair->nodes[0],
		 lat, bw, s1->id, pair->nodes[1]);
	  
	  for (uint h = 0; h < pair->n_help_nodes; h++)
	    {
	      printf("%-2u < ", pair->help_nodes[h]);
	    }
	  printf("\n");
	  if (p < (level->n_pairs - 1))
	    {
	      printf("\t");
	    }
	}
    }
}



/* 0: optimize for bandwidth (i.e., the last pair, appears at lvl 0 and lvl 2 as well.
   1: have the nodes of each lvl appear at the left side of the pairs on each lvl*/
#define MCTOP_NODE_TREE_TYPE     1

  /* create a node tree for hierarchical algorithms */
mctop_node_tree_t*
mctop_alloc_node_tree_create(mctop_alloc_t* alloc, mctop_type_t barrier_for)
{
  const uint n_sockets = alloc->n_sockets;
  if ((n_sockets) & (n_sockets - 1))
    {
      fprintf(stderr, "MCTOP Warning: %u nodes, not power of 2. The tree will be inbalanced!\n", n_sockets);
    }
  int n_lvls = floor_log_2(n_sockets);

  mctop_node_tree_t* nt = mctop_node_tree_alloc(n_lvls);
  nt->alloc = alloc;
  nt->n_nodes = alloc->n_sockets;

  darray_t* socket_ids = darray_create(), * sids_avail = darray_create(), * sids_to_match = darray_create();

  for (int s = 0; s < n_sockets; s++)
    {
      darray_add(socket_ids, alloc->sockets[s]->id);
    }

  for (int lvl = 0; lvl < n_lvls; lvl++)
    {
      uint n_part = 2 << lvl;

      darray_empty(sids_avail);
      darray_copy(sids_avail, socket_ids);
      if (lvl == 0)
	{
	  darray_add(sids_to_match, darray_get(sids_avail, 0));
	}
      else
	{
	  darray_empty(sids_to_match);
	  for (int n = 0; n < (n_part / 2); n++)
	    {
	      const uint sid = mctop_nt_get_nth_socket(nt, lvl - 1, n);
	      darray_add(sids_to_match, sid);
	    }
	}

#if MCTOP_NODE_TREE_TYPE == 1
      darray_remove_all(sids_avail, sids_to_match);
#endif

      for (int i = 0; i < n_part; i += 2)
	{
	  uintptr_t sid;
	  darray_pop(sids_to_match, &sid); 

#if MCTOP_NODE_TREE_TYPE == 0
	  if (!darray_remove(sids_avail, sid))
	    {
	      darray_pop(sids_avail, &sid);
	    }
#endif

	  socket_t* left = mctop_id_get_hwc_gs(alloc->topo, sid);

	  for (int j = 0; j < left->n_siblings; j++)
	    {
	      socket_t* right = mctop_sibling_get_other_socket(left->siblings_in[j], left);
#if MCTOP_NODE_TREE_TYPE == 0
	      if (!darray_elem_is_at(sids_to_match, right->id, 0) && 
	      	  darray_remove(sids_avail, right->id))
#elif MCTOP_NODE_TREE_TYPE == 1
		if (darray_remove(sids_avail, right->id))
#endif
		  {
		    mctop_nt_pair_t* pair = mctop_nt_get_pair(nt, lvl, (i + 1)/2);
		    pair->nodes[0] = mctop_alloc_socket_seq_id(alloc, left->id);
		    pair->socket_ids[0] = left->id;
		    pair->nodes[1] = mctop_alloc_socket_seq_id(alloc, right->id);
		    pair->socket_ids[1] = right->id;
		    break;
		  }
	    }
	}
      uint n_avail = darray_get_num_elems(sids_avail);
      mctop_nt_lvl_t* level = mctop_nt_get_level(nt, lvl);
      const uint n_avail_per_pair = n_avail / level->n_pairs;
      for (uint p = 0; p < level->n_pairs; p++)
	{
	  mctop_nt_pair_t* pair = mctop_nt_get_pair(nt, lvl, p);
	  pair->n_help_nodes = n_avail_per_pair;
	  pair->help_nodes = malloc_assert(n_avail_per_pair * sizeof(uint));
	}

      uint* counters = calloc_assert(level->n_pairs, sizeof(uint));

      while (darray_get_num_elems(sids_avail) > 0)
	{
	  for (uint p = 0; p < level->n_pairs && n_avail > 0; p++)
	    {
	      mctop_nt_pair_t* pair = mctop_nt_get_pair(nt, lvl, p);
	      socket_t* socket = mctop_alloc_get_nth_socket(alloc, pair->nodes[0]);
	      for (uint s = 0; s < socket->n_siblings; s++)
		{
		  sibling_t* sibl = socket->siblings_in[s];
		  socket_t* other = mctop_sibling_get_other_socket(sibl, socket);
		  if (darray_remove(sids_avail, other->id))
		    {
		      pair->help_nodes[counters[p]++] = mctop_alloc_socket_seq_id(alloc, other->id);
		      break;
		    }
		}
	    }
	}
      free(counters);
    }

  nt->barrier_for = barrier_for;
  mctop_node_tree_add_barriers(nt, barrier_for);
  mctop_node_tree_add_id_offsets(nt, barrier_for);

  nt->scratchpad = calloc_assert(nt->n_nodes, sizeof(void*));

  darray_free(sids_to_match);
  darray_free(sids_avail);
  darray_free(socket_ids);

  return nt;
}


static mctop_nt_pair_t*
mctop_nt_get_pair_for_node(mctop_node_tree_t* nt, const uint lvl, const uint node)
{
  if (lvl >= nt->n_levels)
    {
      return NULL;
    }
  mctop_nt_lvl_t* level = &nt->levels[lvl];
  for (int p = 0; p < level->n_pairs; p++)
    {
      mctop_nt_pair_t* pair = &level->pairs[p];
      if (pair->nodes[0] == node || pair->nodes[1] == node)
	{
	  return pair;
	}
    }
  return NULL;
}

static mctop_nt_pair_t*
mctop_nt_get_pair_for_node_all(mctop_node_tree_t* nt, const uint lvl, const uint node, uint* spot)
{
  if (lvl >= nt->n_levels)
    {
      return NULL;
    }

  mctop_nt_lvl_t* level = &nt->levels[lvl];
  for (int p = 0; p < level->n_pairs; p++)
    {
      mctop_nt_pair_t* pair = &level->pairs[p];
      if (pair->nodes[0] == node || pair->nodes[1] == node)
	{
	  return pair;
	}
      for (uint h = 0; h < pair->n_help_nodes; h++)
	{
	  if (pair->help_nodes[h] == node)
	    {
	      *spot = h;
	      return pair;
	    }
	}
    }
  return NULL;
}

uint
mctop_node_tree_get_work_description(mctop_node_tree_t* nt, const uint lvl, mctop_node_tree_work_t* ntw)
{
  const uint node = mctop_alloc_thread_node_id();
  uint spot = 0;
  mctop_nt_pair_t* pair;
  if (nt->barrier_for == EVERYONE_HWC || nt->barrier_for == EVERYONE_CORE)
    {
      pair = mctop_nt_get_pair_for_node_all(nt, lvl, node, &spot);
    }
  else
    {
      pair = mctop_nt_get_pair_for_node(nt, lvl, node);
    }

  if (pair == NULL)		/* no work for me! */
    {
      return 0;
    }

  if (ntw != NULL)
    {
      uint other_node;
      if (pair->nodes[0] == node)
	{
	  ntw->node_role = DESTINATION;
	  other_node = pair->nodes[1];
	  ntw->id_offset = pair->node_id_offsets[0];
	}
      else if (pair->nodes[1] == node)
	{
	  ntw->node_role = SOURCE_ONLY;
	  other_node = pair->nodes[0];
	  ntw->id_offset = pair->node_id_offsets[1];
	}
      else
	{
	  ntw->node_role = HELPING;
	  other_node = pair->nodes[0];
	  ntw->id_offset = pair->help_node_id_offsets[spot];
	}
    
      ntw->other_node = other_node;
      ntw->destination = pair->nodes[0];
      ntw->source = pair->nodes[1];
      /* ntw->num_hw_contexts_my_node = nt->alloc->n_hwcs_per_socket[node]; */
      /* ntw->num_hw_contexts_other_node = nt->alloc->n_hwcs_per_socket[other_node]; */
      ntw->num_hw_contexts = pair->n_hwcs; //ntw->num_hw_contexts_my_node + ntw->num_hw_contexts_other_node;
    }
  return 1;
}


inline uint
mctop_node_tree_get_num_levels(mctop_node_tree_t* nt)
{
  return nt->n_levels;
}

uint
mctop_node_tree_get_final_dest_node(mctop_node_tree_t* nt)
{
  if (nt->n_nodes == 1)
    {
      return 0;
    }
  return nt->levels[0].pairs[0].nodes[0];
}

static mctop_barrier_t*
mctop_node_tree_get_barrier_level(mctop_node_tree_t* nt, const uint lvl)
{
  return nt->levels[lvl].barrier;
}

uint
mctop_node_tree_barrier_wait(mctop_node_tree_t* nt, const uint lvl)
{
  if (!mctop_node_tree_get_work_description(nt, lvl, NULL))
    {
      return 0;
    }
  
  mctop_barrier_t* level_barrier = mctop_node_tree_get_barrier_level(nt, lvl);
  mctop_barrier_wait(level_barrier);
  return 1;
}


void
mctop_node_tree_barrier_wait_all(mctop_node_tree_t* nt)
{
  mctop_barrier_wait(nt->barrier);
}

void*
mctop_node_tree_scratchpad_set(mctop_node_tree_t* nt, const uint node, void* new)
{
  void* cur = nt->scratchpad[node];
  nt->scratchpad[node] = new;
  return cur;
}
 
void*
mctop_node_tree_scratchpad_get(mctop_node_tree_t* nt, const uint node)
{
  return nt->scratchpad[node];
}

