#include <mctop.h>
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


typedef struct mctop_nt_pair
{
  uint nodes[2];   /* 0 is the **receiving** node */
  uint socket_ids[2];
  /* more */
} mctop_nt_pair_t;

typedef struct mctop_nt_lvl
{
  uint n_nodes;
  uint n_pairs;
  mctop_nt_pair_t* pairs;
} mctop_nt_lvl_t;

typedef struct mctop_node_tree
{
  mctop_alloc_t* alloc;
  uint n_levels;
  uint n_nodes;
  mctop_nt_lvl_t* levels;
} mctop_node_tree_t;


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

  /* create a node tree for hierarchical algorithms */
void
mctop_alloc_node_tree_create(mctop_alloc_t* alloc)
{
  const uint n_sockets = alloc->n_sockets;
  if ((n_sockets) & (n_sockets - 1))
    {
      fprintf(stderr, "MCTOP Warning: %u nodes, not power of 2. The tree will be inbalanced!\n", n_sockets);
    }
  int n_lvls = floor_log_2(n_sockets);

  mctop_node_tree_t* nt = mctop_node_tree_alloc(n_lvls);
  nt->alloc = alloc;

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
	      //	      darray_add(sids_to_match, s[lvl - 1][n]->id);
	      darray_add(sids_to_match, sid);
	    }
	}

      for (int i = 0; i < n_part; i += 2)
	{
	  uintptr_t sid;
	  darray_pop(sids_to_match, &sid); 

	  if (!darray_remove(sids_avail, sid))
	    {
	      darray_pop(sids_avail, &sid); 
	    }

	  socket_t* left = mctop_id_get_hwc_gs(alloc->topo, sid);

	  for (int j = 0; j < left->n_siblings; j++)
	    {
	      socket_t* right = mctop_sibling_get_other_socket(left->siblings_in[j], left);
	      if (!darray_elem_is_at(sids_to_match, right->id, 0) &&
		  darray_remove(sids_avail, right->id))
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

  mctop_node_tree_print(nt);

  darray_free(sids_to_match);
  darray_free(sids_avail);
  darray_free(socket_ids);
}
