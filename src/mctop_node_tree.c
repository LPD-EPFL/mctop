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
      nt->levels[l].pairs = malloc_assert(n_pairs * sizeof(mctop_nt_pair_t));
    }

  return nt;
}

void
mctop_node_tree_free(mctop_node_tree_t* nt)
{
  for (int l = 0; l < nt->n_levels; l++)
    {
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


void
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
	  if (barrier_for == EVERYONE)
	    {
	      n_wait_lvl = nt->alloc->n_hwcs;
	    }
	  mctop_barrier_init(nt->levels[l].barrier, n_wait_lvl);
	  /* printf(" LVL %d : %zu threads\n", l, n_wait_lvl); */
	}

      darray_free(nodes);
}

mctop_nt_pair_t*
mctop_nt_get_pair(mctop_node_tree_t* nt, const uint lvl, const uint sid)
{
  return &nt->levels[lvl].pairs[sid];
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
	  socket_t* s0 = mctop_alloc_get_nth_socket(nt->alloc, level->pairs[p].nodes[0]);
	  socket_t* s1 = mctop_alloc_get_nth_socket(nt->alloc, level->pairs[p].nodes[1]);
	  double bw = mctop_socket_get_bw_to(s0, s1);
	  uint lat = mctop_ids_get_latency(nt->alloc->topo, s0->id, s1->id);
	    
	  printf("%u[%u] <--[%-3u cy / %-4.1f GB/s]-- %u[%u]\n", s0->id, level->pairs[p].nodes[0],
		 lat, bw, s1->id, level->pairs[p].nodes[1]);
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
  assert(barrier_for == CORE || barrier_for == HW_CONTEXT || barrier_for == EVERYONE);
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
    }

  nt->barrier_for = barrier_for;
  mctop_node_tree_add_barriers(nt, barrier_for);
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

uint
mctop_node_tree_get_work_description(mctop_node_tree_t* nt, const uint lvl, mctop_node_tree_work_t* ntw)
{
  const uint node = mctop_alloc_thread_node_id();
  mctop_nt_pair_t* pair = mctop_nt_get_pair_for_node(nt, lvl, node);
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
	}
      else
	{
	  ntw->node_role = SOURCE_ONLY;
	  other_node = pair->nodes[0];
	}
    
      ntw->other_node = other_node;
      ntw->num_hw_contexts_my_node = nt->alloc->n_hwcs_per_socket[node];
      ntw->num_hw_contexts_other_node = nt->alloc->n_hwcs_per_socket[other_node];
      ntw->num_hw_contexts = ntw->num_hw_contexts_my_node + ntw->num_hw_contexts_other_node;
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

