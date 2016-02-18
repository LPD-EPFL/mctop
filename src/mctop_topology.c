#include <mctop.h>
#include <darray.h>

cdf_cluster_t* mctop_topology_infer_clustering(uint64_t** lat_table_norm, const size_t N);

void
mctop_topology_create(uint64_t** lat_table_norm, const size_t N, cdf_cluster_t* cc)
{
  int free_cc = 0;
  if (cc == NULL)
    {
      cc = mctop_topology_infer_clustering(lat_table_norm, N);
      free_cc = 1;
    }

  uint8_t* processed = malloc_assert(N * sizeof(uint8_t));
  darray_t* group = darray_create();
  for (int lvl = 1; lvl < cc->n_clusters; lvl++)
    {
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
	      	  if (w != x && w != y) /* w is neither x or y that are being checked */
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
	  darray_empty(group);
	}
    }
  darray_free(group);
  free(processed);
  if (free_cc)
    {
      cdf_cluster_free(cc);
    }
}


cdf_cluster_t*
mctop_topology_infer_clustering(uint64_t** lat_table_norm, const size_t N)
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
