#include <mctop.h>

static mctop_queue_t* mctop_queue_create_on_seq(mctop_alloc_t* alloc, const uint sid);


/* sort an array of uint, but do not modify the actual array. Instead, return
 an array with the indexes corresponding to the sorted uint array. */
static uint*
mctop_sort_uint_index(const uint* arr, const uint n)
{
  uint* sorted = malloc_assert(n * sizeof(uint));
  for (uint i = 0; i < n; i++)
    {
      sorted[i] = i;
    }
  for (uint i = 0; i < n; i++)
    {
      for (uint j = i + 1; j < n; j++)
	{
	  if (arr[sorted[j]] < arr[sorted[i]])
	    {
	      uint tmp = sorted[j];
	      sorted[j] = sorted[i];
	      sorted[i] = tmp;
	    }
	}
    }
  return sorted;
}

mctop_wq_t*
mctop_wq_create(mctop_alloc_t* alloc)
{
  uint n_sockets = mctop_alloc_get_num_sockets(alloc);
  mctop_wq_t* wq = malloc_assert(sizeof(mctop_wq_t) +
				 n_sockets * sizeof(mctop_queue_t*));
  wq->alloc = alloc;
  wq->n_queues = n_sockets;

  for (int i = 0; i < wq->n_queues; i++)
    {
      wq->queues[i] = mctop_queue_create_on_seq(alloc, i);
    }

  for (int i = 0; i < wq->n_queues; i++)
    {
      uint lats[wq->n_queues];
      socket_t* sock_i = mctop_alloc_get_nth_socket(alloc, i);
      for (int j = 0; j < wq->n_queues; j++)
	{
	  uint rj = (i + j) % wq->n_queues;
	  socket_t* sock_j = mctop_alloc_get_nth_socket(alloc, rj);
	  lats[j] = mctop_ids_get_latency(alloc->topo, sock_i->id, sock_j->id);
	}

      uint* indexes = mctop_sort_uint_index(lats, wq->n_queues);

      for (int j = 1; j < wq->n_queues; j++)
	{
	  uint rj = (i + indexes[j]) % wq->n_queues;
	  wq->queues[i]->next_q[j - 1] = rj;
	}
      wq->queues[i]->next_q[wq->n_queues - 1] = i;
      free(indexes);
    }

  return wq;
}

void 
mctop_wq_print(mctop_wq_t* wq)
{
  mctop_alloc_t* alloc = wq->alloc;

  printf("## MCTOP Work Queue -- %u per-node queues\n", wq->n_queues);
  for (int i = 0; i < wq->n_queues; i++)
    {
      socket_t* s = mctop_alloc_get_nth_socket(alloc, i);
      printf("# Q#%-2u - Socket #%u : Steal: ", i, s->id);
      mctop_queue_t* q = wq->queues[i];
      for (int j = 0; j < (wq->n_queues - 1); j++)
	{
	  socket_t* f = mctop_alloc_get_nth_socket(alloc, q->next_q[j]);
	  printf("%u -> ", f->id);
	}
      printf("\n");
    }
}



static void mctop_queue_free(mctop_queue_t* q, const uint n_sockets);

void
mctop_wq_free(mctop_wq_t* wq)
{
  for (int i = 0; i < wq->n_queues; i++)
    {
      mctop_queue_free(wq->queues[i], wq->n_queues);
    }
  free(wq);
}

static mctop_queue_t*
mctop_queue_create_on_seq(mctop_alloc_t* alloc, const uint sid)
{
  size_t qsize = sizeof(mctop_queue_t) + (alloc->n_sockets * sizeof(uint));
  mctop_queue_t* q = mctop_alloc_malloc_on_nth_socket(alloc,
						      sid,
						      qsize);
  q->lock = 0;
  q->size = 0;
  mctop_qnode_t* n = malloc_assert(sizeof(mctop_qnode_t));
  n->next = n->data = NULL;
  q->head = q->tail = n;
  return q;
}

static void
mctop_queue_free(mctop_queue_t* q, const uint n_sockets)
{
  free(q->head);
  size_t qsize = sizeof(mctop_queue_t) + (n_sockets * sizeof(uint));
  mctop_alloc_malloc_free(q, qsize);
}

/* ******************************************************************************** */
/* low-level enqueue / dequeue */
/* ******************************************************************************** */

#ifdef __sparc__		/* SPARC */
#  include <atomic.h>
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define PAUSE()    __asm volatile("rd    %%ccr, %%g0\n\t" ::: "memory")
#elif defined(__tile__)		/* TILER */
#  include <arch/atomic.h>
#  include <arch/cycle.h>
#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)
#  define PAUSE()    cycle_relax()
#elif __x86_64__
#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define PAUSE()    __asm volatile ("pause")
#else
#  error "Unsupported Architecture"
#endif


static inline void
mctop_queue_lock(mctop_queue_t* qu)
{
  while (CAS_U64(&qu->lock, 0, 1))
    {
      do
	{
	  PAUSE();
	} while (qu->lock == 1);
    }
}

static inline void
mctop_queue_unlock(mctop_queue_t* qu)
{
  qu->lock = 0;
}

void
mctop_queue_enqueue(mctop_queue_t* qu, void* data)
{
  mctop_qnode_t* node = malloc_assert(sizeof(mctop_qnode_t));
  node->data = data;

  mctop_queue_lock(qu);
  qu->tail->next = node;
  qu->tail = node;

  qu->size++;
  mctop_queue_unlock(qu);
}

void*
mctop_queue_dequeue(mctop_queue_t* qu)
{
  if (qu->size == 0)
    {
      return NULL;
    }

  mctop_queue_lock(qu);
  if (qu->size == 0)
    {
      mctop_queue_unlock(qu);
      return NULL;
    }

  mctop_qnode_t* node = qu->head;
  mctop_qnode_t* head_new = node->next;
  void* data = head_new->data;

  qu->head = head_new;

  qu->size--;
  mctop_queue_unlock(qu);

  free(node);

  return data;
}

/* ******************************************************************************** */
/* high-level enqueue / dequeue */
/* ******************************************************************************** */

#define MCTOP_WQ_PROF 0
#if MCTOP_WQ_PROF == 1
#  include <helper.h>
#  define GETTICKS_IN(s) ticks s = getticks();
#  define GETTICKS_SUM(sum, add) sum += add;
#  define INC(x) x++
__thread ticks __mctop_wq_prof_enqueue_n = 0,
  __mctop_wq_prof_enqueue_t = 0,
  __mctop_wq_prof_dequeue_local_n = 0,
  __mctop_wq_prof_dequeue_local_t = 0,
  __mctop_wq_prof_dequeue_n[8] = { 1, 1, 1, 1, 1, 1, 1, 1 },
  __mctop_wq_prof_dequeue_t[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

#else
#  define GETTICKS_IN(s) 
#  define GETTICKS_SUM(sum, add)
#  define INC(x)
#endif

void
mctop_wq_enqueue(mctop_wq_t* wq, void* data)
{
  INC(__mctop_wq_prof_enqueue_n);
  GETTICKS_IN(__s);

  mctop_queue_t* qu = wq->queues[mctop_alloc_get_node_seq_id()];
  mctop_queue_enqueue(qu, data);

  GETTICKS_IN(__e);
  GETTICKS_SUM(__mctop_wq_prof_enqueue_t, __e - __s);
}

void*
mctop_wq_dequeue(mctop_wq_t* wq)
{
  GETTICKS_IN(__s);

  mctop_queue_t* qu = wq->queues[mctop_alloc_get_node_seq_id()];
  void* data = mctop_queue_dequeue(qu);
  if (data != NULL)
    {
      GETTICKS_IN(__e);
      GETTICKS_SUM(__mctop_wq_prof_dequeue_local_t, __e - __s);
      INC(__mctop_wq_prof_dequeue_local_n);

      return data;
    }

  for (int q = 0; q < wq->n_queues; q++)
    {
      GETTICKS_IN(__s);
      const uint next = qu->next_q[q];
      mctop_queue_t* qun = wq->queues[next];
      void* data = mctop_queue_dequeue(qun);
      if (data != NULL)
	{
	  GETTICKS_IN(__e);
	  GETTICKS_SUM(__mctop_wq_prof_dequeue_t[q], __e - __s);
	  INC(__mctop_wq_prof_dequeue_n[q]);

	  return data;
	}
    }

  return NULL;
}

void
mctop_wq_stats_print(mctop_wq_t* wq)
{
#if MCTOP_WQ_PROF == 1
  for (int i = 0; i < 8; i++)
    {
      if (__mctop_wq_prof_dequeue_n[i] > 1)
	{
	  __mctop_wq_prof_dequeue_n[i]--;
	}
    }

  if (wq->n_queues == 1)
    {
      printf("WQ@%u #Enq: %-6zu = %4zu cy | #Deq: %-6zu = %4zu cy\n",
	     mctop_alloc_get_local_node(),
	     __mctop_wq_prof_enqueue_n, __mctop_wq_prof_enqueue_t / __mctop_wq_prof_enqueue_n,
	     __mctop_wq_prof_dequeue_local_n, __mctop_wq_prof_dequeue_local_t / __mctop_wq_prof_dequeue_local_n);
    }
  else if (wq->n_queues <= 2)
    {
      printf("WQ@%u #Enq: %-6zu = %4zu cy | #Deq: %-6zu = %4zu cy"
	     " [ %-4zu = %4zu ]\n",
	     mctop_alloc_get_local_node(),
	     __mctop_wq_prof_enqueue_n, __mctop_wq_prof_enqueue_t / __mctop_wq_prof_enqueue_n,
	     __mctop_wq_prof_dequeue_local_n, __mctop_wq_prof_dequeue_local_t / __mctop_wq_prof_dequeue_local_n,
	     __mctop_wq_prof_dequeue_n[0], __mctop_wq_prof_dequeue_t[0] / __mctop_wq_prof_dequeue_n[0]);
    }
  else if (wq->n_queues <= 4)
    {
      printf("WQ@%u #Enq: %-6zu = %4zu cy | #Deq: %-6zu = %4zu cy"
	     " [ %-4zu = %4zu cy | %-4zu = %4zu cy | %-4zu = %4zu cy ]\n",
	     mctop_alloc_get_local_node(),
	     __mctop_wq_prof_enqueue_n, __mctop_wq_prof_enqueue_t / __mctop_wq_prof_enqueue_n,
	     __mctop_wq_prof_dequeue_local_n, __mctop_wq_prof_dequeue_local_t / __mctop_wq_prof_dequeue_local_n,
	     __mctop_wq_prof_dequeue_n[0], __mctop_wq_prof_dequeue_t[0] / __mctop_wq_prof_dequeue_n[0],
	     __mctop_wq_prof_dequeue_n[1], __mctop_wq_prof_dequeue_t[1] / __mctop_wq_prof_dequeue_n[1],
	     __mctop_wq_prof_dequeue_n[2], __mctop_wq_prof_dequeue_t[2] / __mctop_wq_prof_dequeue_n[2]);
    }
  else
    {
      printf("WQ@%u #Enq: %-6zu = %4zu cy | #Deq: %-6zu = %4zu cy"
	     " [ %-4zu = %4zu cy | %-4zu = %4zu cy | %-4zu = %4zu cy | %-4zu = %4zu cy | \n"
	     "                                                       %-4zu = %4zu cy | %-4zu = %4zu cy | %-4zu = %4zu cy ]\n",
	     mctop_alloc_get_local_node(),
	     __mctop_wq_prof_enqueue_n, __mctop_wq_prof_enqueue_t / __mctop_wq_prof_enqueue_n,
	     __mctop_wq_prof_dequeue_local_n, __mctop_wq_prof_dequeue_local_t / __mctop_wq_prof_dequeue_local_n,
	     __mctop_wq_prof_dequeue_n[0], __mctop_wq_prof_dequeue_t[0] / __mctop_wq_prof_dequeue_n[0],
	     __mctop_wq_prof_dequeue_n[1], __mctop_wq_prof_dequeue_t[1] / __mctop_wq_prof_dequeue_n[1],
	     __mctop_wq_prof_dequeue_n[2], __mctop_wq_prof_dequeue_t[2] / __mctop_wq_prof_dequeue_n[2],
	     __mctop_wq_prof_dequeue_n[3], __mctop_wq_prof_dequeue_t[3] / __mctop_wq_prof_dequeue_n[3],
	     __mctop_wq_prof_dequeue_n[4], __mctop_wq_prof_dequeue_t[4] / __mctop_wq_prof_dequeue_n[4],
	     __mctop_wq_prof_dequeue_n[5], __mctop_wq_prof_dequeue_t[5] / __mctop_wq_prof_dequeue_n[5],
	     __mctop_wq_prof_dequeue_n[6], __mctop_wq_prof_dequeue_t[6] / __mctop_wq_prof_dequeue_n[6]);
    }
#endif
}

