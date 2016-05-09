#ifndef __H_MCTOP_ALLOC__
#define __H_MCTOP_ALLOC__

#include <mctop.h>

#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif

  /* ******************************************************************************** */
  /* Thread barriers */
  /* ******************************************************************************** */

  typedef pthread_barrier_t               mctop_barrier_t;
#define mctop_barrier_init(barrier, n)  pthread_barrier_init(barrier, NULL, n)
#define mctop_barrier_wait(barrier)     pthread_barrier_wait(barrier)
#define mctop_barrier_destroy(barrier)  pthread_barrier_destroy(barrier)

  /* ******************************************************************************** */
  /* MCTOP Allocator */
  /* ******************************************************************************** */

#define MCTOP_ALLOC_NUM 11
  typedef enum 
    {
      MCTOP_ALLOC_NONE,		    /* Do not pin anything! */
      MCTOP_ALLOC_SEQUENTIAL,	    /* Return HWC ids 0, 1, 2, 3, ... */
      MCTOP_ALLOC_MIN_LAT_HWCS,       /* Minimize latency across used sockets. Use HWCs of same core first  */
      MCTOP_ALLOC_MIN_LAT_CORES_HWCS, /* Minimize latency across used sockets. Use physical cores of a socket first,
					 HWCs of that socket after and then proceed to the next socket. */
      MCTOP_ALLOC_MIN_LAT_CORES,      /* Minimize latency across used sockets. Use physical cores first and once all 
					 of them have been used start using HWCs */
      MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_HWCS, but balances hwcs across the used sockets. */
      MCTOP_ALLOC_MIN_LAT_CORES_HWCS_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_CORES_HWCS, 
						 but balances hwcs across the used sockets.
						 HWCs of that socket after and then proceed to the next socket. */
      MCTOP_ALLOC_MIN_LAT_CORES_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_CORES, but balances cores across the used sockets. */
      MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS,  /* Maximize bandwidth to local nodes. Allocate HWCs round robin in terms of sockets. 
					   Use HWCs of the same core first.*/
      MCTOP_ALLOC_BW_ROUND_ROBIN_CORES, /* Maximize bandwidth to local nodes. Allocate HWCs round robin in terms of sockets.
					   Use physical cores first.*/
      MCTOP_ALLOC_BW_BOUND,	    /* Maximize bandwidth to local nodes and calculates the number of cores that are 
				       required to saturate the bandwidth on each node. */
    } mctop_alloc_policy;

  typedef struct mctop_alloc
  {
    mctop_t* topo;
    mctop_alloc_policy policy;
    uint n_hwcs;
    uint n_cores;
    uint n_sockets;
    socket_t** sockets;
    uint* n_hwcs_per_socket;
    uint* n_cores_per_socket;
    uint* node_to_nth_socket;
    double* bw_proportions;
    double* pow_max_pac;
    double* pow_max_tot;
    uint max_latency;
    double min_bandwidth;
    uint* hwcs;
    uint* core_sids;		/* seq core ids that correspond to hwcs */
    volatile uint n_hwcs_used;
    volatile uint8_t* hwcs_used;
#ifdef __x86_64__
    struct bitmask* hwcs_all;
#else
    lgrp_id_t hwcs_all;
#endif

    mctop_barrier_t** socket_barriers;
    mctop_barrier_t** socket_barriers_cores;
    mctop_barrier_t* global_barrier;
  } mctop_alloc_t;

  struct mctop_alloc_pool;

  typedef struct mctop_thread_info
  {
    struct mctop_alloc_pool* alloc_pool;
    mctop_alloc_t* alloc;
    uint is_pinned;
    int id;
    uint hwc_id;
    uint local_node;
    uint nth_socket;

    uint nth_hwc_in_core;
    uint nth_hwc_in_socket;
    uint nth_core_socket;
    uint nth_core;
  } mctop_thread_info_t;

  __attribute__((unused)) static const char* mctop_alloc_policy_desc[MCTOP_ALLOC_NUM] = 
    { 
      "MCTOP_ALLOC_NONE",
      "MCTOP_ALLOC_SEQUENTIAL",
      "MCTOP_ALLOC_MIN_LAT_HWCS",
      "MCTOP_ALLOC_MIN_LAT_CORES_HWCS",
      "MCTOP_ALLOC_MIN_LAT_CORES",
      "MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE",
      "MCTOP_ALLOC_MIN_LAT_CORES_HWCS_BALANCE",
      "MCTOP_ALLOC_MIN_LAT_CORES_BALANCE",
      "MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS",
      "MCTOP_ALLOC_BW_ROUND_ROBIN_CORES",
      "MCTOP_ALLOC_BW_BOUND",
    };

#define MCTOP_ALLOC_ALL           -1

  /* Alloc structs manipulation *************************************************************************************** */

  /* mctop_alloc_create params 
   *
   * n_hwcs == MCTOP_ALLOC_ALL : set n_hwcs to the # of hw contexts of the processor
   *
   * MCTOP_ALLOC_SEQUENTIAL           : n_hwcs = total # hw contexts
   *
   * MCTOP_ALLOC_MIN_LAT_HWCS       
   * MCTOP_ALLOC_MIN_LAT_CORES_HWCS
   * MCTOP_ALLOC_MIN_LAT_CORES        : n_hwcs = total # hw contexts / n_config = limit the # of hw contexts per socket
   *                                    pass MCTOP_ALLOC_ALL to get all hw contexts per socket
   *
   * MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE          
   * MCTOP_ALLOC_MIN_LAT_CORES_BALANCE : n_hwcs = total # hw contexts / n_config = ignored
   *                                     pass MCTOP_ALLOC_ALL to get all hw contexts per socket
   *
   * MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS 
   * MCTOP_ALLOC_BW_ROUND_ROBIN_CORES : n_hwcs = total # hw contexts / n_config = how many sockets to use
   * 
   * MCTOP_ALLOC_BW_BOUND             : n_hwcs = how many extra hw contexts to allocate per socket
   *                                    n_config = how many sockets to use
   */
  mctop_alloc_t* mctop_alloc_create(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy);
  /* no barriers for simple !!! */
  mctop_alloc_t* mctop_alloc_create_simple(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy);

  void mctop_alloc_free(mctop_alloc_t* alloc);
  void mctop_alloc_print(mctop_alloc_t* alloc);
  void mctop_alloc_print_short(mctop_alloc_t* alloc);
  void mctop_alloc_help();

  /* Thread functions ************************************************************************************************** */

  int mctop_alloc_pin(mctop_alloc_t* alloc); /* does NOT fix numa node + cannot repin (only if ALL threads unpin before any
					      starts pinning again */
  int mctop_alloc_pin_plus(mctop_alloc_t* alloc); /* mctop_alloc_pin + repin possible */
  int mctop_alloc_pin_simple(mctop_alloc_t* alloc); /* pin plus without stats, such as core ids, smt ids, etc. */

  int mctop_alloc_unpin();
  int mctop_alloc_pin_nth_socket(mctop_alloc_t* alloc, const uint nth);
  int mctop_alloc_pin_all(mctop_alloc_t* alloc);

  void mctop_alloc_barrier_wait_all(mctop_alloc_t* alloc); /* wait for ALL threads handled by alloc to cross */
  void mctop_alloc_barrier_wait_node(mctop_alloc_t* alloc); /* wait for the threads of the node/socket to cross */
  void mctop_alloc_barrier_wait_node_cores(mctop_alloc_t* alloc); /* wait for the threads of the node/socket to cross */

  void mctop_alloc_thread_print();     /* print current threads pin details */
  uint mctop_alloc_thread_is_pinned(); /* is thread pinned? */
  mctop_alloc_t* mctop_alloc_thread_get_alloc(); /* return alloc used */

  uint mctop_alloc_thread_is_pinned(); /* is thread pinned? */
  int mctop_alloc_thread_id();	     /* thread id (NOT hw context id). -1 if thread is not pinned. */
  int mctop_alloc_thread_hw_context_id(); /* hw context id (the id the we use for set_cpu() */
  int mctop_alloc_thread_core_id(); /* sequential core id -- depends on the allocator*/
  uint mctop_alloc_thread_incore_id(); /* seq id of the hw context of this thread
						       in it's core (0=1st hyperthread, 1=2nd?, ..) */
  uint mctop_alloc_thread_insocket_id(); /* seq id of the hw context of this thread in it's socket */
  uint mctop_alloc_thread_core_insocket_id(); /* seq id of the core of this thread in it's socket */
  int mctop_alloc_thread_local_node(); /* local NUMA node of thread */
  int mctop_alloc_thread_node_id();   /* sequence id of the node that this thread is using. For example, the allocator
					  could be using sockets [3, 7]. Socket 3 is node seq id 0 and 7 seq id 1. */

  uint mctop_alloc_thread_is_node_leader(); /* mctop_alloc_thread_insocket_id() == 0 */
  uint mctop_alloc_thread_is_node_last(); /* mctop_alloc_thread_insocket_id() == (n_hwcs in socket - 1) */

  /* Queries *********************************************************************************************************** */

  mctop_alloc_policy mctop_alloc_get_policy(mctop_alloc_t* alloc);
  uint mctop_alloc_get_num_hw_contexts(mctop_alloc_t* alloc);
  uint mctop_alloc_get_num_hw_contexts_node(mctop_alloc_t* alloc, const uint sid);
  uint mctop_alloc_get_num_cores_node(mctop_alloc_t* alloc, const uint sid);
  const char* mctop_alloc_get_policy_desc(mctop_alloc_t* alloc);
  double mctop_alloc_get_min_bandwidth(mctop_alloc_t* alloc);
  uint mctop_alloc_get_max_latency(mctop_alloc_t* alloc);
  uint mctop_alloc_get_nth_hw_context(mctop_alloc_t* alloc, const uint nth);
  socket_t* mctop_alloc_get_nth_socket(mctop_alloc_t* alloc, const uint nth);

  /* # of sockets / nodes that the allocator uses */
  uint mctop_alloc_get_num_sockets(mctop_alloc_t* alloc);
  /* get the OS NUMA node id for the nth socket of the allocator */
  uint mctop_alloc_get_nth_node(mctop_alloc_t* alloc, const uint nth);
  /* get the seq id of the socket that corresponds to NUMA node node */
  uint mctop_alloc_node_to_nth_socket(mctop_alloc_t* alloc, const uint node);
  /* get the seq id in mctop of the mctop id of a socket! -1 if not available */
  int mctop_alloc_socket_seq_id(mctop_alloc_t* alloc, const uint socket_mctop_id);

  /* allocates and initialized a libnuma bitmask to be used in numa_alloc_interleaved_subset */
  struct bitmask* mctop_alloc_create_nodemask(mctop_alloc_t* alloc);

  double mctop_alloc_get_nth_socket_bandwidth_proportion(mctop_alloc_t* alloc, const uint nth);

  uint mctop_alloc_ids_get_latency(mctop_alloc_t* alloc, const uint id0, const uint id1);

  /* allocate with libnuma on the node of the nth socket of the allocator */
  void* mctop_alloc_malloc_on_nth_socket(mctop_alloc_t* alloc, const uint nth, const size_t size);
  void mctop_alloc_malloc_free(void* mem, const size_t size);


  /* ******************************************************************************** */
  /* MCTOP Allocator pool */
  /* ******************************************************************************** */

#define MCTOP_ALLOC_POOL_MAX_N     8

  typedef struct mctop_alloc_args
  {
    int n_hwcs;
    int n_config;
    mctop_alloc_t* alloc;
  } mctop_alloc_args_t;


  typedef struct mctop_alloc_pool
  {
    mctop_t* topo;
    mctop_alloc_args_t* allocs[MCTOP_ALLOC_NUM][MCTOP_ALLOC_POOL_MAX_N];
    volatile mctop_alloc_t* current_alloc;
  } mctop_alloc_pool_t;

  mctop_alloc_pool_t* mctop_alloc_pool_create_empty(mctop_t* topo);
  mctop_alloc_pool_t* mctop_alloc_pool_create(mctop_t* topo, const int n_hwcs, const int n_cnf, mctop_alloc_policy plc);
  void mctop_alloc_pool_free(mctop_alloc_pool_t* ap);


  void mctop_alloc_pool_set_alloc(mctop_alloc_pool_t* ap, const int n_hwcs,
				  const int n_config, mctop_alloc_policy policy);
  

  int mctop_alloc_pool_pin_on_nth_socket(mctop_alloc_pool_t* ap, const uint n);
  int mctop_alloc_pool_pin(mctop_alloc_pool_t* ap);
  /* pin on the nth core of the current allocator */
  int mctop_alloc_pool_pin_on(mctop_alloc_pool_t* ap, const uint seq_id);

  /* ******************************************************************************** */
  /* Node merge tree */
  /* ******************************************************************************** */

#define EVERYONE_HWC  HWC_GROUP
#define EVERYONE_CORE (EVERYONE_HWC+1)

  typedef struct mctop_nt_pair
  {
    uint n_hwcs;
    uint nodes[2];   /* 0 is the **receiving** node */
    uint socket_ids[2];
    uint node_id_offsets[2];
    uint n_help_nodes;
    uint* help_nodes;
    uint* help_node_id_offsets;
  } mctop_nt_pair_t;

  typedef struct mctop_nt_lvl
  {
    uint n_nodes;
    uint n_pairs;
    mctop_nt_pair_t* pairs;
    mctop_barrier_t* barrier;
  } mctop_nt_lvl_t;

  typedef struct mctop_node_tree
  {
    mctop_alloc_t* alloc;
    uint n_levels;
    uint n_nodes;
    mctop_nt_lvl_t* levels;
    mctop_barrier_t* barrier;
    mctop_type_t barrier_for;
    void** scratchpad;		/* share with the threads of your node */
  } mctop_node_tree_t;

  typedef enum
    {
      DESTINATION,		/* merged data go on this node */
      SOURCE_ONLY,		/* this nodes just gives data  */
      HELPING,			/* help a pair of nodes to merge */
    } mctop_node_tree_role;
    
  typedef struct mctop_node_tree_work
  {
    mctop_node_tree_role node_role; /* DESTINATION or SOURCE_ONLY */
    uint other_node;
    uint destination;		/* my destination node */
    uint source;		/* my source node */
    uint id_offset;		/* my id offset to use in the computations */
    uint num_hw_contexts;
    /* uint num_hw_contexts_my_node; */
    /* uint num_hw_contexts_other_node; */
  } mctop_node_tree_work_t;

  /*  barrier_for = HW_CONTEXT : initialize barriers for the # of hw contexts of the node
      barrier_for = CORE       : initialize barriers for the # of cores of the node
   */
  mctop_node_tree_t* mctop_alloc_node_tree_create(mctop_alloc_t* alloc, mctop_type_t barrier_for);
  void mctop_node_tree_print(mctop_node_tree_t* nt);
  void mctop_node_tree_free(mctop_node_tree_t* nt);

  /* sets nt->scratchpad[node] and returns previous val */
  void* mctop_node_tree_scratchpad_set(mctop_node_tree_t* nt, const uint node, void* mem);
  void* mctop_node_tree_scratchpad_get(mctop_node_tree_t* nt, const uint node);

  uint mctop_node_tree_get_num_levels(mctop_node_tree_t* nt);

  uint mctop_node_tree_get_final_dest_node(mctop_node_tree_t* nt);

  /* returns 0 if the node of does not have work at this level */
  uint mctop_node_tree_get_work_description(mctop_node_tree_t* nt, const uint lvl,
					    mctop_node_tree_work_t* ntw);
  /* returns 0 if the node of does not have work at this level */
  uint mctop_node_tree_barrier_wait(mctop_node_tree_t* nt, const uint lvl);

  /* waits on all threads */
  void mctop_node_tree_barrier_wait_all(mctop_node_tree_t* nt);


  /* ******************************************************************************** */
  /* Work Queues */
  /* ******************************************************************************** */

  typedef struct mctop_wq
  {
    mctop_alloc_t* alloc;
    uint n_queues;
    volatile uint32_t n_entered;
    volatile uint32_t n_exited;    
    struct mctop_queue* queues[0];	/* or * ? */
  } mctop_wq_t;


#define MCTOP_ALIGNED(al) __attribute__((aligned(al)))

  typedef struct MCTOP_ALIGNED(64) mctop_queue
  {
    volatile uint64_t lock;
    volatile size_t size;
    struct mctop_qnode* head;
    struct mctop_qnode* tail;
    volatile uint8_t padding[64 - sizeof(uint64_t) - sizeof(size_t) - 2 * sizeof(struct mctop_qnode*)];
    uint next_q[0];
  } mctop_queue_t;

  typedef  struct MCTOP_ALIGNED(64) mctop_qnode
  {
    struct mctop_qnode* next;
    const void* data;
  } mctop_qnode_t;

  mctop_wq_t* mctop_wq_create(mctop_alloc_t* alloc);
  void mctop_wq_free(mctop_wq_t* wq);

  void mctop_wq_print(mctop_wq_t* wq);
  void mctop_wq_stats_print(mctop_wq_t* wq);

  void mctop_wq_enqueue(mctop_wq_t* wq, const void* data); /* Use local node queue. */
  void mctop_wq_enqueue_nth_socket(mctop_wq_t* wq, const uint nth, const void* data); /* Use nth queue. */
  void mctop_wq_enqueue_node(mctop_wq_t* wq, const uint node, const void* data);     /* Use queue corresponding to "node" 
											NUMA node */

  void* mctop_wq_dequeue(mctop_wq_t* wq); /* Try to dequeue from local. If empty, try the remote ones. 
					     If empty, retry local once more. */
  void* mctop_wq_dequeue_local(mctop_wq_t* wq); /* Try to dequeue from local only. */
  void* mctop_wq_dequeue_remote(mctop_wq_t* wq); /* Try to dequeue from remote ones only. */

  size_t mctop_wq_get_size_atomic(mctop_wq_t* wq); /* Lock all queues and get the total current size. */

  uint mctop_wq_thread_enter(mctop_wq_t* wq);   /* inform the others that you are working on WQ. Returns 1 if last thread. */
  uint mctop_wq_thread_exit(mctop_wq_t* wq);	/* inform the others that you stopped working on WQ. Returns 1 if last thread. */
  uint mctop_wq_is_last_thread(mctop_wq_t* wq);	/* Returns 1 if it's the last active thread. */

#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_ALLOC__ */


