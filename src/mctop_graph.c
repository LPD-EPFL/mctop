#include <mctop.h>
#include <stdarg.h>

const char* dot_prefix = "dot";

static void
print2(FILE* ofp, const char* format, ...)
{
  va_list args0, args1;
  va_start(args0, format);
  va_copy(args1, args0);
  vfprintf(stdout, format, args0);
  va_end(args1);

  if (ofp != NULL)
    {
      va_start(args1, format);
      vfprintf(ofp, format, args1);
      va_end(args1);
    }
}

static void
dot_tab(FILE* ofp, const uint n_tabs)
{
  for (int t = 0; t < n_tabs; t++)
    {
      print2(ofp, "\t");
    }
}

static void
dot_tab_print(FILE* ofp, const uint n_tabs, const char* print)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "%s", print);
}

static void
dot_graph(FILE* ofp, const char* title)
{
  print2(ofp, "graph mctop_%s\n{\n", title);
  print2(ofp, "\tlabelloc=\"t\";\n");
  print2(ofp, "\tcompound=true;\n");
  print2(ofp, "\tnode [shape=record];\n");
  print2(ofp, "\tnode [color=gray];\n");
  print2(ofp, "\tedge [fontcolor=blue];\n");

}

static void
dot_subgraph(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "subgraph cluster_%u\n", id);
}

static void
dot_socket(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "label=\"Socket #%u (Latency: %d)\";\n", mctop_id_no_lvl(id), lat);
}

static void
dot_gs(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u", id);
}


static void
dot_gs_label(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u [label=\"", id);
}

static void
dot_gs_link(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u\", weight=\"%u\"]\n", id, id, lat, lat);
}

void
mctopo_dot_graph_socket_plot(mctopo_t* topo)
{
  char out_file[100];
  sprintf(out_file, "dot/%s_socket.dot", dot_prefix);
  FILE* ofp = fopen(out_file, "w+");
  if (ofp == NULL)
    {
      fprintf(stderr, "MCTOP Warning: Cannot open output file %s! Will only plot at stdout.\n", out_file);
    }

  dot_graph(ofp, "socket");
  for (int lvl = 1; lvl <= topo->socket_level; lvl++)
    {
      if (lvl == 1 && lvl < topo->socket_level) /* have sub-groups in socket */
	{
	  hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, lvl);
	  while (gs != NULL)
	    {
	      dot_gs_label(ofp, 1, gs->id);
	      for (int i = 0; i < gs->n_children; i++)
		{
		  if (i > 0)
		    {
		      print2(ofp, "|");
		    }
		  print2(ofp, "%u", gs->children[i]->id);
		}
	      print2(ofp, "\"];\n");
	      dot_gs_link(ofp, 1, gs->id, gs->latency);

	      gs = gs->next;
	    }
	}
      else if (lvl == 1)
	{
	  hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, lvl);
	  while (gs != NULL)
	    {
	      dot_subgraph(ofp, 1, gs->id);
	      dot_tab_print(ofp, 1, "{\n");
	      dot_socket(ofp, 2, gs->id, gs->latency);
	      dot_gs_label(ofp, 2, gs->id);
	      for (int i = 0; i < gs->n_children; i++)
		{
		  if (i > 0)
		    {
		      print2(ofp, "|");
		    }
		  print2(ofp, "%u", gs->children[i]->id);
		}
	      print2(ofp, "\"];\n");
	      dot_tab_print(ofp, 1, "}\n");
	      gs = gs->next;
	    }
	}
      else
	{
	  hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, lvl);
	  while (gs != NULL)
	    {
	      dot_subgraph(ofp, 1, gs->id);
	      dot_tab_print(ofp, 1, "{\n");
	      dot_socket(ofp, 2, gs->id, gs->latency);
	      for (int i = 0; i < gs->n_children; i++)
		{
		  dot_gs(ofp, 2, gs->children[i]->id);
		  print2(ofp, "\n");
		}
	      dot_tab_print(ofp, 1, "}\n");
	      gs = gs->next;
	    }	  
	}

      if (lvl == topo->socket_level)
	{
	  dot_tab_print(ofp, 1, "node [color=gold4];\n");

	  if (topo->has_mem >= LATENCY)
	    {
	      hwc_gs_t* gs = mctop_get_first_gs_at_lvl(topo, lvl);
	      while (gs != NULL)
		{
		  dot_tab(ofp, 1); print2(ofp, "//Memory latencies node %u\n", gs->id);
		  for (int i = 0; i < gs->n_nodes; i++)
		    {
		      dot_tab(ofp, 1);
		      if (i == gs->local_node)
			{
			  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\", color=\"red\"];\n", 
				 i, gs->id, i, gs->mem_latencies[i]);
			}
		      else
			{
			  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\"];\n", 
				 i, gs->id, i, gs->mem_latencies[i]);
			}

		      dot_tab(ofp, 1);
		      if (topo->socket_level == 1)
			{
			  if (topo->has_mem == BANDWIDTH)
			    {
			      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
				     i, gs->id, gs->id, gs->id, gs->mem_bandwidths[i]);
			    }
			  else
			    {
			      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
				     i, gs->id, gs->id, gs->id);
			    }
			}
		      else
			{
			  if (topo->has_mem == BANDWIDTH)
			    {
			      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
				     i, gs->id, gs->children[0]->id, gs->id, gs->mem_bandwidths[i]);
			    }
			  else
			    {
			      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
				     i, gs->id, gs->children[0]->id, gs->id);
			    }
			}
		    }
		  gs = gs->next;
		}
	    }
	}
    }
  print2(ofp, "}\n");

  if (ofp != NULL)
    {
      fclose(ofp);
    }
}

void
mctopo_dot_graph_plot(mctopo_t* topo)
{
  mctopo_dot_graph_socket_plot(topo);
}
