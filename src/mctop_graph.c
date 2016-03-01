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
dot_graph(FILE* ofp, const char* title, const uint id)
{
  print2(ofp, "graph mctop_%s_%u\n{\n", title,id);
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
  dot_tab_print(ofp, n_tabs, "{\n");
}

static void
dot_socket(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "label=\"Socket #%u (Latency: %d)\";\n", mctop_id_no_lvl(id), lat);
}

static void
dot_label(FILE* ofp, const uint n_tabs, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "label=\"%u cycles\";\n", lat);
}

static void
dot_gs_label(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u [label=\"", id);
}

static void
dot_gs_label_id(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u [label=\"%u\"];\n", id, mctop_id_no_lvl(id));
}

static void
dot_gs_link(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u\", weight=\"%u\"]\n", id, id, lat, lat);
}

static void
dot_gss_link(FILE* ofp, const uint n_tabs, const uint id0, const uint id1, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u\", weight=\"%u\"]\n", id0, id1, lat, lat);
}

void
dot_gs_recurse(FILE* ofp, hwc_gs_t* gs, const uint n_tabs)
{
  if (gs->level == 1)
    {
      dot_gs_label(ofp, n_tabs, gs->id);
      for (int i = 0; i < gs->n_children; i++)
	{
	  if (i > 0)
	    {
	      print2(ofp, "|");
	    }
	  print2(ofp, "%u", gs->children[i]->id);
	}
      print2(ofp, "\"];\n");
      if (gs->type != SOCKET)
	{
	  dot_gs_link(ofp, n_tabs, gs->id, gs->latency);
	}
    }
  else 
    {
      const uint new_subgraph = gs->type != SOCKET;
      if (new_subgraph)
	{
	  dot_subgraph(ofp, n_tabs, gs->id);
	  dot_label(ofp, n_tabs + 1, gs->latency);
	}
      for (int i = gs->n_children - 1; i >= 0; i--)
	{
	  dot_gs_recurse(ofp, gs->children[i], n_tabs + 1);
	}
      if (new_subgraph)
	{
	  dot_tab_print(ofp, n_tabs, "}\n");
	}
    }
}

void
mctopo_dot_graph_intra_socket_plot(mctopo_t* topo)
{
  char out_file[100];
  sprintf(out_file, "dot/%s_intra_socket.dot", dot_prefix);
  FILE* ofp = fopen(out_file, "w+");
  if (ofp == NULL)
    {
      fprintf(stderr, "MCTOP Warning: Cannot open output file %s! Will only plot at stdout.\n", out_file);
    }

  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = &topo->sockets[s];
      dot_graph(ofp, "intra_socket", s);
      dot_subgraph(ofp, 1, socket->id);
      dot_socket(ofp, 2, socket->id, socket->latency);
      dot_gs_recurse(ofp, socket, 1);
      dot_tab_print(ofp, 1, "}\n");

      dot_tab_print(ofp, 1, "node [color=gold4];\n");

      if (topo->has_mem >= LATENCY)
	{
	  dot_tab(ofp, 1); print2(ofp, "//Memory latencies node %u\n", socket->id);
	  for (int i = 0; i < socket->n_nodes; i++)
	    {
	      dot_tab(ofp, 1);
	      if (i == socket->local_node)
		{
		  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\", color=\"red\"];\n", 
			 i, socket->id, i, socket->mem_latencies[i]);
		}
	      else
		{
		  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\"];\n", 
			 i, socket->id, i, socket->mem_latencies[i]);
		}

	      dot_tab(ofp, 1);
	      if (socket->level == 1)
		{
		  if (topo->has_mem == BANDWIDTH)
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
			     i, socket->id, socket->id, socket->id, socket->mem_bandwidths[i]);
		    }
		  else
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
			     i, socket->id, socket->id, socket->id);
		    }
		}
	      else
		{
		  if (topo->has_mem == BANDWIDTH)
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
			     i, socket->id, socket->children[0]->id, socket->id, socket->mem_bandwidths[i]);
		    }
		  else
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
			     i, socket->id, socket->children[0]->id, socket->id);
		    }
		}
	    }
	}
      print2(ofp, "}\n");
    }

  if (ofp != NULL)
    {
      fclose(ofp);
    }
}

void
mctopo_dot_graph_cross_socket_plot(mctopo_t* topo)
{
  char out_file[100];
  sprintf(out_file, "dot/%s_cross_socket.dot", dot_prefix);
  FILE* ofp = fopen(out_file, "w+");
  if (ofp == NULL)
    {
      fprintf(stderr, "MCTOP Warning: Cannot open output file %s! Will only plot at stdout.\n", out_file);
    }
  dot_graph(ofp, "cross_socket", 0);

  dot_tab(ofp, 1); print2(ofp, "// Socket       lvl %u (max lvl %u)\n", topo->socket_level, topo->n_levels);
  for (int i = 0; i < topo->n_sockets; i++)
    {
      dot_gs_label_id(ofp, 1, topo->sockets[i].id);
    }

  for (int lvl = topo->socket_level + 1; lvl < topo->n_levels; lvl++)
    {
      dot_tab(ofp, 1); print2(ofp, "// Cross-socket lvl %u (max lvl %u)\n", lvl, topo->n_levels);
      for (int i = 0; i < topo->n_siblings; i++)
	{
	  sibling_t* sibling = topo->siblings[i];
	  if (sibling->level == lvl)
	    {
	      dot_gss_link(ofp, 1, sibling->left->id, sibling->right->id, sibling->latency);
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
  mctopo_dot_graph_intra_socket_plot(topo);
  /* mctopo_dot_graph_cross_socket_plot(topo); */
}
