

#include "offload_server.h"

#define MAX_OFFLOAD_THREAD_IN_NODE 128

extern __thread CPUArchState *thread_env;


// used along with pthread_cond, indicate whether the page required by the execution thread is received.
static int page_recv_flag[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_mutex_t page_recv_mutex[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_cond_t page_recv_cond[MAX_OFFLOAD_THREAD_IN_NODE];
static abi_ulong page_recv_addr[MAX_OFFLOAD_THREAD_IN_NODE];
static int page_syscall_recv_flag; static pthread_mutex_t page_syscall_recv_mutex; static pthread_cond_t page_syscall_recv_cond;
static int mutex_ready_flag[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_mutex_t mutex_recv_mutex[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_cond_t mutex_recv_cond[MAX_OFFLOAD_THREAD_IN_NODE];
static int cpu_exit_flag; static pthread_mutex_t exit_recv_mutex; static pthread_cond_t exit_recv_cond;
static int syscall_ready_flag[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_mutex_t syscall_recv_mutex[MAX_OFFLOAD_THREAD_IN_NODE]; 
static pthread_cond_t syscall_recv_cond[MAX_OFFLOAD_THREAD_IN_NODE];
abi_long result_global[MAX_OFFLOAD_THREAD_IN_NODE];
int syscall_clone_done;
pthread_mutex_t syscall_clone_mutex;
pthread_cond_t syscall_clone_cond;
static int futex_uaddr_changed_flag; static pthread_mutex_t futex_mutex; static pthread_cond_t futex_cond;
static int exec_ready_to_init; static pthread_mutex_t exec_func_init_mutex; static pthread_cond_t exec_func_init_cond;
static void* exec_segfault_addr[MAX_OFFLOAD_THREAD_IN_NODE]; static void* syscall_segfault_addr;
static int pgfault_time_sum;
static int syscall_time_sum;
pthread_mutex_t page_process_mutex;
pthread_mutex_t server_send_mutex;


/* Init Page Info Table. */
void offload_server_pmd_init(void)
{
	int init_val;
	/* Master has all the privilege at first. */
	if (offload_server_idx > 0)
		init_val = 0;
	else
		init_val = 2;
	for (int i = 0; i < L1_MAP_TABLE_SIZE; i++)
	{
		for (int j = 0; j < L2_MAP_TABLE_SIZE; j++)
		{
			page_map_table_s[i][j].cur_perm = init_val;
			//fprintf(stderr, "%ld", page_map_table[i][j].owner_set.size);
		}
	}
}
/* Get Page info in server(slave) side. */
inline PageMapDesc_server* get_pmd_s(abi_ulong page_addr)
{
	page_addr = PAGE_OF(page_addr);
	page_addr = page_addr >> MAP_PAGE_BITS;
	int index1 = (page_addr >> L1_MAP_TABLE_SHIFT) & (L1_MAP_TABLE_SIZE - 1);
	int index2 = page_addr & (L2_MAP_TABLE_SIZE - 1);
	PageMapDesc_server *pmd = &page_map_table_s[index1][index2];
	return pmd;
}
/* get packet_counter of net_buffer */
static abi_ulong get_number(void)
{
	struct tcp_msg_header tmh = *((struct tcp_msg_header *) net_buffer);
	return tmh.counter;
}

/* get tag of net_buffer */
static uint32_t get_tag(void)
{
	struct tcp_msg_header tmh = *((struct tcp_msg_header *) net_buffer);
	
	return tmh.tag;
}

/* get payloadsize of net_buffer */
static abi_ulong get_size(void)
{
	struct tcp_msg_header tmh = *((struct tcp_msg_header *) net_buffer);
	return tmh.size;
}

/* Initialize socket, socket_mutex, page_recv_cond, page_recv_mutex */
static void offload_server_init(void)
{
	fprintf(stderr, "[offload_server_init]\tindex: %ld\n", offload_server_idx);
	sktfd = socket(AF_INET,SOCK_STREAM, 0);
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(server_port_of(offload_server_idx));
	
	//ip_addr
	sockaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
	pthread_mutex_init(&socket_mutex, NULL);
	int tmp = 1;
	setsockopt(sktfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
	if(bind(sktfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
	{
		printf("[offload_server_init]\tbind socket failed, at port# %ld, errno:%ld\n", server_port_of(offload_server_idx), errno);
        perror("bind");
		exit(0);
	}
	
	fprintf(stderr, "[offload_server_init]\tbind socket, port# %ld\n", server_port_of(offload_server_idx));
	listen(sktfd, 100);
	
	pthread_mutex_init(&page_syscall_recv_mutex,NULL);
	pthread_cond_init(&page_syscall_recv_cond,NULL);
	pthread_mutex_init(&exit_recv_cond,NULL);
	pthread_cond_init(&exit_recv_mutex,NULL);
	for (int i = 0; i < MAX_OFFLOAD_THREAD_IN_NODE; i++) {
		pthread_mutex_init(&syscall_recv_mutex[i],NULL);
		pthread_mutex_init(&syscall_recv_cond[i],NULL);
		pthread_mutex_init(&page_recv_mutex[i], NULL);
		pthread_cond_init(&page_recv_cond[i], NULL);
		pthread_mutex_init(&mutex_recv_mutex[i], NULL);
		pthread_cond_init(&mutex_recv_cond[i], NULL);
	}
	pthread_mutex_init(&futex_mutex, NULL);
	pthread_cond_init(&futex_cond, NULL);
	pthread_mutex_init(&exec_func_init_mutex, NULL);
	pthread_cond_init(&exec_func_init_cond, NULL);
	pgfault_time_sum = 0;
	offload_server_pmd_init();
	pthread_mutex_init(&page_process_mutex, NULL);
	pthread_mutex_init(&server_send_mutex, NULL);

	abi_ulong ret = target_mmap(0xa0000000, 
						0x10000000, PROT_READ|PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	assert(ret == 0xa0000000);
}

static void load_cpu(void)
{
	// copy the CPU struct
	

	//memcpy(thread_env, p, sizeof(CPUARMState));
	extern CPUArchState *env_bak;
	extern __thread CPUArchState *thread_env;
	// if (count_n != 0) {
	// 	thread_env = cpu_copy(env_bak);
	// 	assert(thread_env);
	// }
	// count_n++;
	assert(thread_env);
	*((CPUARMState *) thread_env) = *((CPUARMState *) p);

	p += sizeof(CPUARMState);
	

	fprintf(stderr,"[load_cpu]\tthread_env: %lp\n", thread_env);
	CPUState *cpu = ENV_GET_CPU(thread_env);
	// extern CPUArchState *thread_cpu;
	extern __thread CPUState *thread_cpu;
	thread_cpu = cpu;
	thread_cpu->env_ptr = thread_env;
	fprintf(stderr,"[load_cpu]\tcpu: %lp\n", cpu);
	TaskState *ts1;

	fprintf(stderr,"[load_cpu]\topaque: %lp\n", cpu->opaque);
	ts1 = cpu->opaque;
	fprintf(stderr,"[load_cpu]\tNOW child_tidptr: %lp\n", ts1->child_tidptr);
	/* TaskState is a void*, we've to set it mannually */
	TaskState *ts = g_new0(TaskState, 1);
	*ts = *((TaskState*) p);
	p += sizeof(TaskState);
	cpu->opaque = ts;
	fprintf(stderr,"[load_cpu]\tNOW child_tidptr: %lp\n", ts->child_tidptr);
	assert(ts->child_tidptr);
	/*vfp_set_fpscr(env, *((abi_ulong*) p));
	fprintf(stderr, "fpscr: %ld\n", *((abi_ulong*) p));
	p += sizeof(abi_ulong);
	
    //env->cp15.tpidrro_el0 = client_regs[1] & 0xffffffff;
    env->cp15.tpidrro_el[0] = *((uint64_t*) p);
	fprintf(stderr, "cp15: %ld\n", *((uint64_t*) p));
	p += sizeof(uint64_t);
	
    //cpsr_write(env, client_regs[2], 0xffffffff);	
    cpsr_write(env, *((abi_ulong*) p), 0xffffffff);
	fprintf(stderr, "cpsr: %ld\n", *((abi_ulong*) p));
	p += sizeof(abi_ulong);
	
	
	memcpy(env->vfp.regs, p, sizeof(env->vfp.regs));
	p +=  sizeof(env->vfp.regs);
	
	memcpy(env->regs, p, sizeof(env->regs));
	p +=  sizeof(env->regs);
	fprintf(stderr, "pc: %lx\n",env->regs[15]);*/

	fprintf(stderr, "[load_cpu]\tr0: %ld\n", thread_env->regs[0]);
}

// dump memory_region
static void load_memory_region(void)
{
	// map the memory region:
	
	
	abi_ulong num = *(abi_ulong *)p;
    p += sizeof(abi_ulong);
	fprintf(stderr, "[load_memory_region]\tmemory region of 0%ld\n", num);
    if (num != 0) 
	{
        target_ulong heap_end = *(target_ulong *)p;
        p += sizeof(target_ulong);

        /* initialize heap end */
        target_set_brk(heap_end);
		
        stack_start = *(target_ulong *)p;
        p += sizeof(target_ulong);

        stack_end = *(target_ulong *)p;
        p += sizeof(target_ulong);
    }
	static abi_ulong mapped[50] = {0}, mapped_count = 0, first = 1;
	int mapped_flag = 0;
	for (abi_ulong i = 0; i < num; i++) 
	{
        abi_ulong addr = *(abi_ulong *)p;
        p += sizeof(abi_ulong);
        abi_ulong page_num = *(abi_ulong *)p;
        p += sizeof(abi_ulong);
		abi_ulong flags = *(abi_ulong *)p;
        p += sizeof(abi_ulong);
		abi_ulong len = *(abi_ulong *)p;
        p += sizeof(abi_ulong);
		/* Check if we already mapped */
		mapped_flag = 0;
		for (int j = 0; j < 50; j++) {
			if (mapped[j] == addr) {
				mapped_flag = 1;
				break;
			}
		}
		if (mapped_flag)
			continue;
		/* Now we map the region. */
		fprintf(stderr, "[load_memory_region]\tmemory region: %lx to %lx,  host: %lx to %lx\n", addr, addr + len, g2h(addr), g2h(addr) + len);
		mapped[mapped_count++] = addr;

		abi_ulong ret = target_mmap(addr, page_num * TARGET_PAGE_SIZE, PROT_NONE,
							MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		fprintf(stderr, "[load_memory_region]\tReturn mem addr = %lp\n", ret);
		//assert(ret == addr);
		//mprotect(g2h(addr), page_num * TARGET_PAGE_SIZE, PROT_NONE);
    }
	first = 0;
}

//dump brk
static void load_brk(void)
{
	// copy the brk
	abi_ulong old_brk = *(abi_ulong *)p;
    p += sizeof(abi_ulong);
    abi_ulong current_brk = *(abi_ulong *)p;
    p += sizeof(abi_ulong);
	
    int target_mmap_return;
	static int first = 1;
	if (first) {
		if(old_brk != 0)
		{
			if(current_brk > old_brk)
			{
				target_mmap_return = target_mmap(old_brk, (unsigned int)current_brk - old_brk,
												PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
				mprotect(g2h(old_brk), (unsigned int)current_brk - old_brk, PROT_NONE);
				if (target_mmap_return != old_brk) {
					printf("[load_brk]\ttarget_mmap  failed at start of ProcessOffloadStart, returns %lx\n", target_mmap_return);
					exit(2);
				}
			}
			else
			{
				int ret = target_munmap(current_brk, (unsigned int) old_brk - current_brk);
				if (ret) 
				{
					printf( "[load_brk]\tThe munmap failed at the start of ProcessOffloadStart : %ld \n", ret);
					exit(2);
				}
			}
		}
	}
	first = 0;
}

static void load_binary(void)
{
	abi_ulong binary_start_address, binary_end_address;
	binary_start_address= *(abi_ulong *)p;
	p += sizeof(abi_ulong);
	binary_end_address= *(abi_ulong *)p;
	p += sizeof(abi_ulong);
	static first = 1;
	if (first) {
		fprintf(stderr, "[load_binary]\tmap binary from %lp to %lx\n", binary_start_address, binary_end_address);
		// fprintf(stderr, "[load_binary]\there: %lx %lx %lx\n", g2h(binary_start_address), g2h(binary_end_address), g2h((thread_env->regs[15])));
		int ret;
		ret = mprotect(g2h(binary_start_address), (abi_ulong)binary_end_address - binary_start_address, PROT_READ | PROT_WRITE);
		fprintf(stderr, "[load_binary]\tRet = %lp\n", ret);
		memcpy(g2h(binary_start_address), p, (abi_ulong)binary_end_address - binary_start_address);

		fprintf(stderr, "[DEBUG] checkpoint1\n");
		//disas(stderr, g2h(thread_env->regs[15]), 10);

		mprotect(g2h(binary_start_address), (abi_ulong)binary_end_address - binary_start_address, PROT_READ | PROT_WRITE | PROT_EXEC);
		first = 0;
	}

	fprintf(stderr, "binary begin value: %lp\n", *(abi_ulong*)p);
	p += (abi_ulong)binary_end_address-binary_start_address;
}

/* Initialize execution thread and go to cpu loop */
void exec_func(void)
{
	
	offload_mode = 3;
	static int count_n = 0;
	/* This is an init exec thread.
	*  The initial exec's ID is 0.
	*/
	offload_thread_idx = 0;
	//pthread_mutex_lock(&socket_mutex);
	// static int first = 1;
	// if (first == 1) {
	// 	offload_server_qemu_init();
	// 	first++;
	// }	
	// else {
	// 	//offload_server_extra_init();
	// 	offload_server_idx = first-1;
	// }
	
	fprintf(stderr, "[exec_func]\tguest_base: %lx count_n\n", guest_base, count_n);
	p = net_buffer;
	fprintf(stderr, "[exec_func]\tin exec func\n");
	load_cpu();
	// copy the start function address
	;
	load_memory_region();
	load_brk();
	load_binary();
	// it's go time!
	//fprintf(stderr, "this address: %lx\n", g2h(0x10324));
	fprintf(stderr, "[exec_func]\tready to CPU_LOOP\n");

	// fprintf(stderr, "[exec_func]\tPC: %lp\n", thread_env->regs[15]);
	
	
	// fprintf(stderr, "[exec_func]\tregisters:\n");
	
	// for (int i = 0; i < 16; i++)
	// {
	// 	fprintf(stderr, "[exec_func]\t%lp\n", thread_env->regs[i]);
	// }
	//target_disas(stderr, ENV_GET_CPU(env), env->regs[15], 100);
	//while (1) {;}

	if (count_n != 0) {
		fprintf(stderr, "[exec_func]\tAnother thread!!\n");
		rcu_register_thread();
		tcg_register_thread();
		//sleep(99999);
		cpu_loop(thread_env);
	}
	//pthread_mutex_unlock(&socket_mutex);
	
	count_n++;
	//!! Just debugs
	//sleep(199999);
	cpu_loop(thread_env);
	// here this thread reaches an end
		
	return NULL;
	 
}

// void *extra_exec_thread(void)
// {
// 	extern CPUArchState *env;
// 	/* we create a new CPU instance. */
// 	new_env = cpu_copy(env);
// 	/* Init regs that differ from the parent.  */
// 	cpu_clone_regs(new_env, newsp);
// 	new_cpu = ENV_GET_CPU(new_env);
// 	offload_mode = 6;
// 	extern __thread int offload_thread_idx;
// 	offload_thread_idx = 3;
// 	fprintf(stderr, "[exec_func_init]\tWaiting for informations...\n");
// 	fprintf(stderr, "[exec_func_init]\tStart Initializing... guest_base: %lx\n", guest_base);
// 	p = net_buffer;
// 	fprintf(stderr, "[exec_func_init]\tin exec func\n");
// 	load_cpu();
// 	load_memory_region();
// 	load_brk();
// 	load_binary();
// 	// it's go time!
// 	//fprintf(stderr, "this address: %lx\n", g2h(0x10324));
// 	fprintf(stderr, "[exec_func_init]\tready to CPU_LOOP\n");
// 	fprintf(stderr, "[exec_func_init]\tPC: %lp\n", env->regs[15]);	
// 	fprintf(stderr, "[exec_func_init]\tregisters:\n");	
// 	for (int i = 0; i < 16; i++)
// 	{
// 		fprintf(stderr, "[exec_func_init]\t%lp\n", env->regs[i]);
// 	}
// 	//target_disas(stderr, ENV_GET_CPU(env), env->regs[15], 100);
// 	//while (1) {;}
// 	rcu_register_thread();
// 	tcg_register_thread();
// 	//pthread_mutex_unlock(&socket_mutex);
// 	cpu_loop(env);
// 	// here this thread reaches an end
// 	return NULL;
// }
/* For extra exec. */
void exec_func_init(void)
{
	/* This is an extra exec thread.
	*  Using increasing index as its ID.
	*  Extra exec thread ID starts at 1.
	*  The initial exec is 0.
	*/
	offload_mode = 3;
	extern __thread int offload_thread_idx;
	static int ncount = 0;
	offload_thread_idx = ncount;
	fprintf(stderr, "[exec_func_init]\tWaiting for informations...\n");
	/* Once the thread reaches here, set the exec_read_to_init to 1.
	 * wait the flag to be 2 indicating initialization info is ready. */
	pthread_mutex_lock(&exec_func_init_mutex);
	exec_ready_to_init = 1;
	pthread_cond_broadcast(&exec_func_init_cond);
	while (exec_ready_to_init != 2) {
		fprintf(stderr, "[exec_func_init]\tWaiting for informations...NOT READY%ld\n", exec_ready_to_init);

		pthread_cond_wait(&exec_func_init_cond, &exec_func_init_mutex);
	}
	pthread_mutex_unlock(&exec_func_init_mutex);
	//guest_base = 0x3c00000;

	fprintf(stderr, "[exec_func_init]\tStart Initializing... guest_base: %lx\n", guest_base);

	
	
	p = net_buffer;
	fprintf(stderr, "[exec_func_init]\tin exec func\n");
	load_cpu();
	//sleep(100000);
	// copy the start function address
	if (ncount == 0) {
		load_memory_region();
		load_brk();
		load_binary();
	}
	// it's go time!
	//fprintf(stderr, "this address: %lx\n", g2h(0x10324));
	fprintf(stderr, "[exec_func_init]\tready to CPU_LOOP\n");

	// fprintf(stderr, "[exec_func_init]\tPC: %lp\n", thread_env->regs[15]);
	
	
	fprintf(stderr, "[exec_func_init]\tregisters:\n");
	
	// for (int i = 0; i < 16; i++)
	// {
	// 	fprintf(stderr, "[exec_func_init]\t%lp\n", thread_env->regs[i]);
	// }
	//target_disas(stderr, ENV_GET_CPU(env), env->regs[15], 100);
	//while (1) {;}

	rcu_register_thread();
	tcg_register_thread();
	ncount++;

	pthread_mutex_lock(&exec_func_init_mutex);
	exec_ready_to_init = 3;
	pthread_cond_broadcast(&exec_func_init_cond);
	pthread_mutex_unlock(&exec_func_init_mutex);

	//pthread_mutex_unlock(&socket_mutex);
	cpu_loop(thread_env);
	// here this thread reaches an end
	
		
	return NULL;
	 
}

// this happens when exec reaches exit
void cpu_exit_signal(void)
{
	fprintf(stderr,"[cpu_exit_signal]\tSSSSSSSSSSSSSSSSSIGNAL...\n");
	pthread_mutex_lock(&exit_recv_mutex);
	cpu_exit_flag=1;
	pthread_cond_signal(&exit_recv_cond);
	pthread_mutex_unlock(&exit_recv_mutex);
	fprintf(stderr,"[cpu_exit_signal]\texiting...\n");
}

// to kill exec
void * cpu_killer(void* param)
{
	pthread_t exec_thread = *(pthread_t*) param;
	fprintf(stderr, "[cpu_killer]\tKiller ready\n");
	cpu_exit_flag = 0;
	pthread_mutex_lock(&exit_recv_mutex);
	while (cpu_exit_flag==0)
	{
		pthread_cond_wait(&exit_recv_cond,&exit_recv_mutex);	
	}	
	pthread_mutex_unlock(&exit_recv_mutex);	
	fprintf(stderr, "[cpu_killer]\tTerminiting exec_cpu...:\n");
	//pthread_kill(exec_thread,NULL);
	return NULL;
}

/* create execution thread */
static void offload_process_start(void)
{
	
	static int count = 0;
	pthread_t exec_thread;
	fprintf(stderr, "[offload_process_start]\tcreate exec thread\n");
	if (count == 0 && 0) {
		// pthread_create(&exec_thread, NULL, exec_func, NULL);
		pthread_mutex_lock(&main_exec_mutex);
		main_exec_flag = 1;
		pthread_cond_broadcast(&main_exec_cond);
		pthread_mutex_unlock(&main_exec_mutex);
		count++;
	}
	else {
		pthread_mutex_lock(&exec_func_init_mutex);
		while (exec_ready_to_init != 1) {
			pthread_cond_wait(&exec_func_init_cond, &exec_func_init_mutex);
		}
		exec_ready_to_init = 2;
		pthread_cond_broadcast(&exec_func_init_cond);
		fprintf(stderr, "[offload_process_start]\tWake up please! %ld\n", exec_ready_to_init);
		pthread_mutex_unlock(&exec_func_init_mutex);
		// pthread_create(&exec_thread, NULL, &extra_exec_thread, NULL);
		
	}
	fprintf(stderr, "[offload_process_start]\texec thread created\n");
	{
		pthread_mutex_lock(&exec_func_init_mutex);
		while (exec_ready_to_init != 3) {
			pthread_cond_wait(&exec_func_init_cond, &exec_func_init_mutex);
		}
		fprintf(stderr, "[offload_process_start]\tInit done! %ld\n", exec_ready_to_init);
		pthread_mutex_unlock(&exec_func_init_mutex);
	}
	/*
	pthread_t killer_thread;
	pthread_create(&exec_thread, NULL, cpu_killer, (void*)&exec_thread);
	*/
	//pthread_join(exec_thread, NULL);
	
}

/* send mutex request in order to fetch a free lock 
	|MUTEX_REQUEST|mutex_addr|					*/
void offload_server_send_mutex_request(abi_ulong mutex_addr, abi_ulong cmpv, abi_ulong newv)
{
	//!!!
	//mutex_addr = h2g(mutex_addr);

	char buf[TARGET_PAGE_SIZE * 2];
	char *pp = buf + sizeof(struct tcp_msg_header);
	*((target_ulong *)pp) = (target_ulong)mutex_addr;
	pp += sizeof(target_ulong);
	*((abi_ulong *)pp) = (abi_ulong) offload_server_idx;
	pp += sizeof(abi_ulong);
	*((abi_ulong *)pp) = cmpv;
	pp += sizeof(abi_ulong);
	*((abi_ulong *)pp) = newv;
	pp += sizeof(abi_ulong);
	*((int *)pp) = offload_thread_idx;
	pp += sizeof(int);
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) buf;
	fill_tcp_header(tcp_header, pp - buf - sizeof(struct tcp_msg_header), TAG_OFFLOAD_CMPXCHG_REQUEST);
	/* we should lock first in case verified returns before we sleep!!!!!! */
	pthread_mutex_lock(&mutex_recv_mutex);
	
	int res = autoSend(client_socket, buf, pp - buf, 0);
	if (res < 0)
	{
		printf( "[cmpxchg_request]\tsent mutex request %lp failed\n", mutex_addr);
		exit(0);
	}
	fprintf(stderr, "[cmpxchg_request]\tsent mutex request, mutex addr: %lp, packet %ld, waiting...offload_server_idx=%ld\n", mutex_addr, get_number(), offload_server_idx);
	fprintf(stderr, "[cmpxchg_request]\tcas addr %lp, idx %ld, cmpv %lx, newv %lx\n", mutex_addr, offload_server_idx, cmpv, newv);
	mutex_ready_flag[offload_thread_idx] = 0;
	while (mutex_ready_flag[offload_thread_idx] == 0)
	{
		pthread_cond_wait(&mutex_recv_cond[offload_thread_idx], &mutex_recv_mutex[offload_thread_idx]);
	}
	pthread_mutex_unlock(&mutex_recv_mutex[offload_thread_idx]);
	fprintf(stderr, "[cmpxchg_request]\tsent mutex request, mutex addr: %lp, packet %ld, I'm awake!\n", mutex_addr, get_number());

}

static void offload_server_process_mutex_verified(void)
{
		//pthread_mutex_lock(&socket_mutex);
	p = net_buffer;
	int thread_idx = *((int *) p);
    p += sizeof(int);
	fprintf(stderr, "[offload_server_process_mutex_verified]\twaking up thread %ld->%ld\n",
							offload_server_idx, thread_idx);
	pthread_mutex_lock(&mutex_recv_mutex[thread_idx]);
	mutex_ready_flag[thread_idx] = 1;
	pthread_cond_signal(&mutex_recv_cond[thread_idx]);
	pthread_mutex_unlock(&mutex_recv_mutex[thread_idx]);
	//if (mutex_ready_flag == 0) offload_server_process_mutex_verified();
}

/* send page request |REQUEST|page_addr|perm| */
static void offload_server_send_page_request(target_ulong page_addr, abi_ulong perm)
{
	//pthread_mutex_lock(&socket_mutex);
	fprintf(stderr, ">>>>>>>>> exec# %ld guest_base: %lx\n", offload_server_idx, guest_base);
	char buf[TARGET_PAGE_SIZE * 2];
	/* prepare space for head */
	char *pp = buf + sizeof(struct tcp_msg_header);
	/* page_addr and perm */
	*((target_ulong *) pp) = page_addr;
    pp += sizeof(target_ulong);	
	*((abi_ulong *) pp) = perm;
	pp += sizeof(abi_ulong);
	// fill head with payloadsize and tag
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) buf;
	fill_tcp_header(tcp_header, pp - buf - sizeof(struct tcp_msg_header), TAG_OFFLOAD_PAGE_REQUEST);
	
	int res = autoSend(client_socket, buf, pp - buf, 0);
	if (res < 0)
	{
		printf( "[offload_server_send_page_request]\tsent page %lx request failed\n", page_addr);
		exit(0);
	}
	fprintf(stderr, "[offload_server_send_page_request]\tsent page %lx request, perm: %s, packet#%ld\n", page_addr, perm==1?"READ":"READ|WRITE", get_number());
	//pthread_mutex_unlock(&socket_mutex);
}

/* send page and modify permission to invalidate or shared */
static void offload_process_page_request(void)
{
	//pthread_mutex_lock(&socket_mutex);
	pthread_mutex_lock(&cmpxchg_mutex);
	p = net_buffer;
	
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	
	abi_ulong perm = *((abi_ulong *) p);
	p += sizeof(abi_ulong);
	int client_idx = *((int*) p);
	p += sizeof(int);
	int forwho = *((int*) p);
	p += sizeof(int);
	PageMapDesc_server *pmd = get_pmd_s(page_addr);
	fprintf(stderr, "[offload_process_page_request]\tpage %lx, perm %ld, from %ld, for %ld\n", page_addr, perm, client_idx, forwho);
	// when debug, erase this.
	pmd->cur_perm = 1;
	if (offload_server_idx == 0) {
		pthread_mutex_lock(&master_mprotect_mutex);
	}
	mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ);//prevent writing at this time!!

	//if (page_addr == 0x78000)
	//{
	//	fprintf(stderr, "[offload_process_page_request]\tdebug\t0x78f4c = %ld", *(int *)(g2h(0x78f4c)));
	//}
	///* debug pthread_mutex_struct */
	//if (page_addr == 0x78000)
	//{
	//	fprintf(stderr, "[offload_process_page_request]\tdebug\t__lock0x77f34 = %ld", *(int *)(g2h(0x78f34)));
	//	fprintf(stderr, "[offload_process_page_request]\tdebug\t__count0x77f38 = %ld", *(int *)(g2h(0x78f38)));
	//	fprintf(stderr, "[offload_process_page_request]\tdebug\t__owner0x77f40 = %ld", *(int *)(g2h(0x78f3C)));
	//}
	offload_send_page_content(page_addr, perm, forwho);
	fprintf(stderr, "[offload_process_page_request]\tsent content\n", page_addr, perm);
	/*	if required permission is WRITE|READ,
	*	we won't be able to use it (invalidate)
	*	otherwise it is a shared page (shared)
	*/
	
	if (perm == 2)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_NONE);
		pmd->cur_perm = 0;
	}
	else if (perm == 1)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ);
		pmd->cur_perm = 1;
	}
	if (offload_server_idx == 0) {
		pthread_mutex_unlock(&master_mprotect_mutex);
	}
	
	//pthread_mutex_unlock(&socket_mutex);
	//pthread_mutex_unlock(&socket_mutex);
	// #todo: if the worker can know that this page is during a perm change from exclusive to shared, 
	// then should he at this moment protect this page with PROT_NONE, or wait till the center sends a PAGE_PERM ?
	// mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_NONE);
	pthread_mutex_unlock(&cmpxchg_mutex);
}

/* copy content to page and send ack? */
static void offload_process_page_content(void)
{
	//pthread_mutex_lock(&socket_mutex);
	p = net_buffer;
	
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	
	abi_ulong perm = *((abi_ulong *) p);
	p += sizeof(abi_ulong);
	fprintf(stderr, "[offload_process_page_content]\tcontent: %ld %ld\n", *((uint64_t *) p), *((uint64_t *) p + 555));

	/* protect page and copy content to page */
	mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
	memcpy(g2h(page_addr), p, TARGET_PAGE_SIZE);
	/* debug pthread_mutex_t */
	if (page_addr == 0x78000)
	{
		fprintf(stderr, "[offload_process_page_content]\tdebug\t__lock0x77f34 = %ld", *(int *)(g2h(0x78f34)));
		fprintf(stderr, "[offload_process_page_content]\tdebug\t__count0x77f38 = %ld", *(int *)(g2h(0x78f38)));
		fprintf(stderr, "[offload_process_page_content]\tdebug\t__owner0x77f40 = %ld", *(int *)(g2h(0x78f3C)));
	}
	p += TARGET_PAGE_SIZE;
	if (perm == 2)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
	}
	else if (perm == 1)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ);
	}
	fprintf(stderr, "[offload_process_page_content]\tpage %lx perm: %s\n", page_addr, perm==1?"READ":"WRITE|READ");
	// wake up the execution thread upon this required page.
	offload_page_recv_wake_up_thread(page_addr, perm);
	offload_send_page_ack(page_addr, perm);
}

void offload_page_recv_wake_up_thread(abi_ulong page_addr, int perm)
{
	pthread_mutex_lock(&page_process_mutex);
	int i = 0;
	for (i = 0; i < MAX_OFFLOAD_THREAD_IN_NODE; i++) {
		if (page_addr == exec_segfault_addr[i]) {
			fprintf(stderr, "[offload_page_recv_wake_up_thread]\twaking up exec %ld->%ld\n",
								offload_server_idx, i);
			pthread_mutex_lock(&page_recv_mutex[i]);
			page_recv_flag[i] = 1;
			pthread_cond_broadcast(&page_recv_cond[i]);
			pthread_mutex_unlock(&page_recv_mutex[i]);
		}
	}
	if (page_addr == syscall_segfault_addr)
	{
		fprintf(stderr, "[offload_page_recv_wake_up_thread]\twaking up syscall\n");
		pthread_mutex_lock(&page_syscall_recv_mutex);
		page_syscall_recv_flag = 1;
		pthread_cond_broadcast(&page_syscall_recv_cond);
		pthread_mutex_unlock(&page_syscall_recv_mutex);
	}
	/* Update server's page map. -1 means we don't know the perm so don't chage it.*/
	if (perm >= 0) {
		PageMapDesc_server* pmd = get_pmd_s(page_addr);
		pmd->cur_perm = perm;
	}
	pthread_mutex_unlock(&page_process_mutex);
}

/* send |CONTENT|page|perm|content| */
static void offload_send_page_content(target_ulong page_addr, abi_ulong perm, int forwho)
{
	/* prepare space for head */
	char buf[TARGET_PAGE_SIZE * 2];
	char *p = buf + sizeof(struct tcp_msg_header);
	/* fill addr and perm */
	*((target_ulong *) p) = page_addr;
    p += sizeof(target_ulong);
	*((abi_ulong *) p) = perm;
	p += sizeof(abi_ulong);
	*((int*) p) = forwho;
	p += sizeof(int);
    /* followed by page content (size = TARGET_PAGE_SIZE) */
	fprintf(stderr, "[DEBUG]\tPOINT1\n");
	//TODO: 如果是2就直接disable了 如果是1就發送。
	//mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
	fprintf(stderr, "[DEBUG]\tPOINT1.5\n");
	memcpy(p, g2h(page_addr), TARGET_PAGE_SIZE);
	fprintf(stderr, "[DEBUG]\tPOINT2\n");
    p += TARGET_PAGE_SIZE;
	/* fill head */
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) buf;
	fill_tcp_header(tcp_header, p - buf - sizeof(struct tcp_msg_header), TAG_OFFLOAD_PAGE_CONTENT);
	fprintf(stderr, "[DEBUG]\tPOINT3\n");
	int res = autoSend(client_socket, buf, p - buf, 0);
	if (res < 0)
	{
		printf( "[offload_send_page_content]\tsent page %lx content failed\n", page_addr);
		exit(0);
	}
	fprintf(stderr, "[offload_send_page_content]\tsent page %lx content, perm%ld, packet#%ld\n", page_addr, perm, get_number());
}


/* send |ACK|page|perm| */
static void offload_send_page_ack(target_ulong page_addr, abi_ulong perm)
{
	//pthread_mutex_lock(&socket_mutex);
	/* prepare space for head */
	p = BUFFER_PAYLOAD_P;
	/* fill addr and perm */
	*((target_ulong *) p) = page_addr;
    p += sizeof(target_ulong);
	*((abi_ulong *) p) = perm;
	p += sizeof(abi_ulong);
	/* fill head, tag = ack */
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) net_buffer;
	fill_tcp_header(tcp_header, p - net_buffer - sizeof(struct tcp_msg_header), TAG_OFFLOAD_PAGE_ACK);
	int res = autoSend(client_socket, net_buffer, p - net_buffer, 0);
	if (res < 0)
	{
		printf("[offload_send_page_ack]\tsent page %lx ack failed\n", page_addr);
		exit(0);
	}
	fprintf(stderr, "[offload_send_page_ack]\tsent page %lx ack with perm: %s\n", page_addr, perm==1?"READ":"WRITE|READ");
	//pthread_mutex_unlock(&socket_mutex);
}

#define PAGE_SHIFT 12
#define VPTPTR 0xfffffffe00000000UL
static inline unsigned long
pt_index(unsigned long addr, int level)
{
	return (addr >> (PAGE_SHIFT + (10 * level))) & 0x3ff;
}

void offload_send_page_request_and_wait(abi_ulong page_addr, int perm)
{
	//TODO So maybe we deal with syscall in the future (must implement pmd_init)
	//Check if we already have the page.
	pthread_mutex_lock(&page_process_mutex);
	PageMapDesc_server *pmd = get_pmd_s(page_addr);
	if (pmd->cur_perm >= perm) {
		//fprintf(stderr, "[offload_send_page_request_and_wait]\tI think we already have the page.\n");
		pthread_mutex_unlock(&page_process_mutex);
		return;
	}
	if (offload_mode != 4)
	{		
		
		pthread_mutex_lock(&page_recv_mutex[offload_thread_idx]);
		//!!! Dangerous! If others have sent this and is waiting, just don not send again. But race condition could happen.
		int have_already_requested = 0;
		int i;
		{
			for (i = 0; i < MAX_OFFLOAD_THREAD_IN_NODE; i++) {
				if (page_recv_flag[i] == 0 && exec_segfault_addr[i] == page_addr && page_addr > 0) {
					have_already_requested = 1;
					break;
				}
			}
		}
		if (!have_already_requested) {
			offload_server_send_page_request(page_addr, perm); 
			fprintf(stderr, "[offload_send_page_request_and_wait]\tsent page REQUEST %lx, wait, sleeping\n", page_addr);
		} else {
			fprintf(stderr, "[offload_send_page_request_and_wait]\tthread %ld already requested.\n", i);
		}
		exec_segfault_addr[offload_thread_idx] = page_addr;
		page_recv_flag[offload_thread_idx] = 0;
		//TODO check if 
		pthread_mutex_unlock(&page_process_mutex);
		while (page_recv_flag[offload_thread_idx] == 0)
		{
			pthread_cond_wait(&page_recv_cond[offload_thread_idx], &page_recv_mutex[offload_thread_idx]);
		}
		exec_segfault_addr[offload_thread_idx] = 0;
		pthread_mutex_unlock(&page_recv_mutex[offload_thread_idx]);
		
		fprintf(stderr, "[offload_send_page_request_and_wait]\tawake\n");
	}
	/* for syscall#0's segfault */
	else
	{
		syscall_segfault_addr = page_addr;
		fprintf(stderr, "[offload_send_page_request_and_wait]\tin syscall segfault\n");
		pthread_mutex_lock(&page_syscall_recv_mutex);
		page_syscall_recv_flag = 0;
		offload_server_send_page_request(page_addr, perm); 
		//offload_server_send_page_request(page_addr, 2);
		fprintf(stderr, "[offload_segfault_handler]\tsent page REQUEST %lx, wait, sleeping\n", page_addr);
		pthread_mutex_unlock(&page_process_mutex);
		while (page_syscall_recv_flag == 0)
		{
			pthread_cond_wait(&page_syscall_recv_cond, &page_syscall_recv_mutex);
		}
		pthread_mutex_unlock(&page_syscall_recv_mutex);
		fprintf(stderr, "[offload_segfault_handler]\tsyscall segfault awake\n");
	}
}
/* send page request; sleep until page is sent back */
int offload_segfault_handler(int host_signum, siginfo_t *pinfo, void *puc)
{
//#define PF_TIME
#ifdef PF_TIME
	struct timeb t, tend;
    ftime(&t);
#endif
    siginfo_t *info = pinfo;
    ucontext_t *uc = (ucontext_t *)puc;
    void* host_addr = info->si_addr;
    //TODO ... do h2g on the host_addr to get the address of the segfault
	
    unsigned long  guest_addr = h2g(host_addr);
	fprintf(stderr, "[offload_segfault_handler]\tguest addr is %lp, host_addr is %lp, pte-0 %lp, pte-1 %lp, pte-2 %lp, VP-2 %lp, VP-1%lp\n", 
			guest_addr, host_addr, pt_index(host_addr, 0), pt_index(host_addr, 1), pt_index(host_addr, 2), pt_index(VPTPTR, 2), pt_index(VPTPTR, 1));
#define PC_sig(context)       ((context)->uc_mcontext.gregs[REG_RIP])
	fprintf(stderr, "[offload_segfault_handler]\tREG_RIP=%lp\n",
					PC_sig(uc));
#define UC_REG(context, reg)	((context)->uc_mcontext.gregs[reg])
	/* REG_10 stores the load/store address. 
	 * which is REG_RBP
	 * REG_11 stores the load/store value, which is REG_RBX.
	 */
	//UC_REG(uc, 10) = 0x1000;
	//UC_REG(uc, REG_RIP) = 
	//for (int i = 0; i < __NGREG; i++) {
	//	fprintf(stderr, "[offload_segfault_handler]\tREG_%ld=%lp\n",
	//					i, UC_REG(uc, i));

	//}
	target_ulong page_addr = guest_addr & TARGET_PAGE_MASK;
    //fprintf(stderr, "\nHost instruction address is %lp\n", uc->uc_mcontext.gregs[REG_RIP]);
    int is_write = ((uc->uc_mcontext.gregs[REG_ERR] & 0x2) != 0);
	//TODO !!!!!!!!!!!!!!!DEBUG
	//is_write = 1;
	fprintf(stderr, "[offload_segfault_handler]\tsegfault on page addr: %lx, perm: %s\n", page_addr, is_write?"WRITE|READ":"READ");
	// sum time on pagefault
	offload_send_page_request_and_wait(page_addr, is_write+1);
	//get_client_page(is_write, guest_page);
	// send page request, sleep until content is sent back.
	//fprintf(stderr, "[offload_segfault_handler]\t%lp value is %lp\n", guest_addr, *(abi_ulong*)(g2h(guest_addr)));
#ifdef PF_TIME
	ftime(&tend);
	int secDiff = tend.time - t.time;
	secDiff *= 1000;
	secDiff += (tend.millitm - t.millitm);
	pgfault_time_sum += secDiff;
	fprintf(stderr, "[offload_segfault_handler]\tbegin: %ld:%ld; end: %ld:%ld, used: %ldms, now total is: %ldms", t.time, t.millitm, tend.time, tend.millitm, secDiff, pgfault_time_sum);
#endif

    return 1;
}
/* send page request; sleep until page is sent back */
int offload_segfault_handler_positive(abi_ulong page_addr, int perm)
{
	offload_send_page_request_and_wait(page_addr & 0xfffff000, perm);
	return 1;
	//TODO self map to avoid extra fetching
	struct timeb t, tend;
	ftime(&t);
	page_addr &= TARGET_PAGE_MASK;
	fprintf(stderr, "[offload_segfault_handler_positive]\tguest addr is %lp\n",
			page_addr);
	int is_write = perm - 1;
	//TODO !!!!!!!!!!!!!!!DEBUG
	//is_write = 1;
	fprintf(stderr, "[offload_segfault_handler_positive]\tsegfault on page addr: %lx, perm: %s\n", page_addr, is_write ? "WRITE|READ" : "READ");
	offload_send_page_request_and_wait(page_addr, is_write+1);

	//fprintf(stderr, "[offload_segfault_handler_positive]\t%lp value is %ld\n", page_addr, *(abi_ulong *)(g2h(page_addr)));
	ftime(&tend);
	int secDiff = tend.time - t.time;
	secDiff *= 1000;
	secDiff += (tend.millitm - t.millitm);
	pgfault_time_sum += secDiff;
	fprintf(stderr, "[offload_segfault_handler_positive]\tbegin: %ld:%ld; end: %ld:%ld, used: %ldms, now total is: %ldms", t.time, t.millitm, tend.time, tend.millitm, secDiff, pgfault_time_sum);

	return 1;
}
/* change permission of page_addr */
static void offload_process_page_perm(void)
{
	//pthread_mutex_lock(&socket_mutex);
	p = net_buffer;
	
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	
	abi_ulong perm = *((abi_ulong *) p);
	p += sizeof(abi_ulong);
	
	if (perm == 1)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ);
	}
	else if (perm == 2)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
	}
	else if (perm == 0)
	{
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_NONE);
	}
	fprintf(stderr, "[offload_process_page_perm]\tCHANGE page %lx perm to %ld\n", page_addr, perm);
	//pthread_mutex_unlock(&socket_mutex);
}

/* wake up exec; change permission to READ|WRITE?? */
static void offload_process_page_upgrade(void)
{
	//pthread_mutex_lock(&socket_mutex);
	p = net_buffer;
	
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	int perm = *((int*) p);
	p += sizeof(int);
	if (perm ==2)
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
	else if(perm == 1)
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_READ);
	else if(perm == 0)
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_NONE);
	
	fprintf(stderr, "[offload_process_page_upgrade]\tpage %lx perm: %s\n", page_addr, perm == 1 ? "READ" : "WRITE|READ");
	// wake up the execution thread upon this required page.
	offload_page_recv_wake_up_thread(page_addr, perm);
	fprintf(stderr, "[offload_process_page_upgrade]\tpage %lx upgrade to %ld\n", page_addr, perm);
	if (perm > 0)
	{

		offload_send_page_ack(page_addr, perm);
	}
	//pthread_mutex_unlock(&socket_mutex);
}


static void offload_server_daemonize(void)
{
	fprintf(stderr, "[offload_server_daemonize]\tstart to daemonize\n");
	
	fprintf(stderr, ">>>>>>>>>>>> server# %ld guest_base: %lx\n", offload_server_idx, guest_base);
	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(client_addr);
	client_socket = accept(sktfd, (struct sockaddr*)&client_addr, &client_addr_size);
	fprintf(stderr, "need to be awaked?\n");
	int flag = 1;
	int result = setsockopt(client_socket,            /* socket affected */
                        IPPROTO_TCP,     /* set option at TCP level */
                        TCP_NODELAY,     /* name of option */
                        (char *) &flag,  /* the cast is historical cruft */
                        sizeof(int));    /* length of option value */
	if (result<0) {
		perror("setsockopt");
		exit(3);
	}
	// 这里需要唤醒吗
	while (1)
	{
		
		/*if (offload_server_idx == 0)
		{
			int res = recv(client_socket, net_buffer, 9999, 0);
			fprintf(stderr, "\nrecv: %ld\n", res);
			exit(0);
		}*/
		fprintf(stderr, "[offload_server_daemonize]\twaiting for new message\n");
		
		//fprintf(stderr, "count addr: %lx\n", g2h(0x7ae34));

		//int res = recv(client_socket, net_buffer, sizeof(struct tcp_msg_header), MSG_WAITALL);
		try_recv(sizeof(struct tcp_msg_header));
		fprintf(stderr, "[offload_server_daemonize]\tgot a new message #%ld\n", get_number());
		int tag = get_tag();
		int size = get_size();
		int packet_counter = get_number();
		fprintf(stderr, "[offload_server_daemonize]\tsize: %ld + %ld\n", sizeof(struct tcp_msg_header), size);
		switch (tag)
		{
			case TAG_OFFLOAD_START:
				fprintf(stderr, "[offload_server_daemonize]\ttag: offload start size: %ld\n", size);
				try_recv(size);
				offload_process_start();
				//fprintf(stderr, "AAAAAAAAAAAAA\n");
				break;
			
			case TAG_OFFLOAD_PAGE_REQUEST:
				fprintf(stderr, "[offload_server_daemonize]\ttag: page request, size: %ld\n", size);
				try_recv(size);
				offload_process_page_request();
				break;
			case TAG_OFFLOAD_PAGE_CONTENT:
				fprintf(stderr, "[offload_server_daemonize]\ttag: page content, size: %ld\n", size);
				try_recv(size);
				offload_process_page_content();
				break;
			
			case TAG_OFFLOAD_PAGE_PERM:
				fprintf(stderr, "[offload_server_daemonize]\ttag: page perm\n");
				try_recv(size);
				offload_process_page_perm();
				break;
			
			case TAG_OFFLOAD_PAGE_UPGRADE:
				fprintf(stderr, "[offload_server_daemonize]\ttag: page upgrade\n");
				try_recv(size);
				offload_process_page_upgrade();
				break;
				
				
			case TAG_OFFLOAD_FUTEX_WAIT_RESULT:
				fprintf(stderr, "[offload_server_daemonize]\ttag: futex wait result\n");
				try_recv(size);
				exit(0);
				offload_server_process_futex_wait_result();
				
				break;
				
			case TAG_OFFLOAD_FUTEX_WAKE_RESULT:
				
				fprintf(stderr, "[offload_server_daemonize]\ttag: futex wake result\n");
				try_recv(size);
				exit(0);
				offload_server_process_futex_wake_result();
				break;

			case TAG_OFFLOAD_CMPXCHG_REQUEST:
				fprintf(stderr, "[offload_server_daemonize]\ttag: cmpxchg request\n");
				try_recv(size);
				//offload_process_mutex_request();
				break;

			case TAG_OFFLOAD_CMPXCHG_VERYFIED:
				fprintf(stderr, "[offload_server_daemonize]\ttag: cmpxchg verified, size = %ld(should be 4)\n", size);
				try_recv(size);
				offload_server_process_mutex_verified();
				break;

			case TAG_OFFLOAD_SYSCALL_RES:
				fprintf(stderr, "[offload_server_daemonize]\ttag: syscall result, size = %ld\n", size);
				try_recv(size);
				//fprintf(stderr, "[offload_server_daemonize]\treceived.\n");
				offload_server_process_syscall_result();
				break;

			case TAG_OFFLOAD_YOUR_TID:
				fprintf(stderr, "[offload_server_daemonize]\ttag: TID, size = %ld\n", size);
				try_recv(size);
				offload_process_tid();
				break;

			case TAG_OFFLOAD_FORK_INFO:
				fprintf(stderr, "[offload_server_daemonize]\ttag: FORK INFO, size = %ld\n", size);
				try_recv(size);
				offload_process_fork_info();
				break;

			case TAG_OFFLOAD_FS_PAGE:
				fprintf(stderr, "[offload_server_daemonize]\ttag: FS Page, size = %ld\n", size);
				try_recv(size);
				offload_server_process_fs_page();
				break;

			case TAG_OFFLOAD_PAGE_WAKEUP:
				fprintf(stderr, "[offload_server_daemonize]\ttag: Page wakeup, size = %ld\n", size);
				try_recv(size);
				offload_server_process_page_wakeup();
				break;

			default:
				printf("[offload_server_daemonize]\tunkown tag: %ld\n", tag);
				exit(0);
				break;
				
		}
	}
}

extern void* offload_center_client_start(void *arg);
void* offload_center_server_start(void *arg)
{
	
	offload_mode = 1;
	offload_server_init();
	fprintf(stderr, "[offload_center_server_start]\tcenter server guest_base: %lx\n", guest_base);
	pthread_t offload_center_client_thread;



	pthread_create(&offload_center_client_thread, NULL, offload_center_client_start, arg);	


	
	offload_server_daemonize();
	return NULL;
}

void offload_server_start(void)
{
	fprintf(stderr, "[offload_server_start]\tstart offload server\n");
	//env = _env;
	offload_server_init();
	offload_server_daemonize();
	
}

void* offload_server_start_thread(void* arg)
{
	offload_mode = 1;
	fprintf(stderr, "[offload_server_start]\tstart offload server\n");
	//env = _env;
	offload_server_init();
	offload_server_daemonize();
	return NULL;
}

void offload_server_send_cmpxchg_start(abi_ulong cas_addr, abi_ulong cmpv, abi_ulong newv)
{
	offload_server_send_mutex_request(cas_addr, cmpv, newv);
}

/* send |MUTEX_DONE|mutex_addr|idx| */
static void offload_send_mutex_done(abi_ulong mutex_addr, abi_ulong nowv)
{
	//pthread_mutex_lock(&socket_mutex);
	/* prepare space for head */
	//p = BUFFER_PAYLOAD_P;
	char buf[TARGET_PAGE_SIZE * 2];
	char *p = buf + sizeof(struct tcp_msg_header);
	*((abi_ulong *) p) = mutex_addr;
    p += sizeof(abi_ulong);
	*((abi_ulong *) p) = offload_server_idx;
	p += sizeof(abi_ulong);
	*((abi_ulong *)p) = nowv;
	p += sizeof(abi_ulong);
	*((int *)p) = offload_thread_idx;
	p += sizeof(int);
	/* fill head, tag = MUTEX_DONE */
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) buf;
	fill_tcp_header(tcp_header, p - buf - sizeof(struct tcp_msg_header), TAG_OFFLOAD_CMPXCHG_DONE);
	int res = autoSend(client_socket, buf, p - buf, 0);
	if (res < 0)
	{
		fprintf(stderr, "[offload_send_mutex_done]\tsent mutex %lp done failed\n", mutex_addr);
		exit(0);
	}
	fprintf(stderr, "[offload_send_mutex_done]\tsent mutex %lp done, nowv %lx from server #%ld\n", mutex_addr, nowv, offload_server_idx);
	//pthread_mutex_unlock(&socket_mutex);
}

void offload_server_send_cmpxchg_end(abi_ulong cas_addr, abi_ulong nowv)
{
	offload_send_mutex_done(cas_addr, nowv);
}

static void offload_server_send_futex_wake_request(target_ulong uaddr, int op, int val, target_ulong timeout, target_ulong uaddr2, int val3)
{
	p = BUFFER_PAYLOAD_P;
	
	*((target_ulong *) p) = uaddr;
    p += sizeof(target_ulong);
	
	*((int *) p) = op;
	p += sizeof(int);
	
	*((int *) p) = val;
	p += sizeof(int);
	
	*((target_ulong *) p) = timeout;
    p += sizeof(target_ulong);
	
	*((target_ulong *) p) = uaddr2;
    p += sizeof(target_ulong);
	
	*((int *) p) = val3;
	p += sizeof(int);
	
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) net_buffer;
	fill_tcp_header(tcp_header, p - net_buffer - sizeof(struct tcp_msg_header), TAG_OFFLOAD_FUTEX_WAKE_REQUEST);
	int res = autoSend(client_socket, net_buffer, p - net_buffer, 0);
	if (res < 0)
	{
		fprintf(stderr, "[offload_server_send_futex_wake_request]\tsent futex wake request failed\n");
		exit(0);
	}
	fprintf(stderr, "[offload_server_send_futex_wake_request]\tsent futex wake request, packet# %ld, uaddr: %lx\n", get_number(), uaddr);
}
static void offload_server_send_futex_wait_request(target_ulong guest_addr, int op, int val, target_ulong timeout, target_ulong uaddr2, int val3)
{
	p = BUFFER_PAYLOAD_P;
	
	*((target_ulong *) p) = guest_addr;
    p += sizeof(target_ulong);
	
	*((int *) p) = op;
	p += sizeof(int);
	fprintf(stderr, "futex op: %ld\n", op);
	*((int *) p) = val;
	p += sizeof(int);
	
	*((target_ulong *) p) = timeout;
    p += sizeof(target_ulong);
	
	*((target_ulong *) p) = uaddr2;
    p += sizeof(target_ulong);
	
	*((int *) p) = val3;
	p += sizeof(int);
	
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) net_buffer;
	fill_tcp_header(tcp_header, p - net_buffer - sizeof(struct tcp_msg_header), TAG_OFFLOAD_FUTEX_WAIT_REQUEST);
	int res = autoSend(client_socket, net_buffer, p - net_buffer, 0);
	if (res < 0)
	{
		fprintf(stderr, "[offload_server_send_futex_wait]\tsent futex wait request failed\n");
		exit(0);
	}
	fprintf(stderr, "[offload_server_send_futex_wake_request]\tsent futex wait request, packet# %ld, uaddr: %lx\n", get_number(), guest_addr);
}

 int offload_server_futex_wait(target_ulong guest_addr, int op, int val, target_ulong timeout, target_ulong uaddr2, int val3)
{
	// can we assume that by the time we encounter a futex syscall,
	// there will be no page request in process?
	// I will take this for now.
	//futex_result = 0;
	
	//page_recv_flag = 0;
	//offload_server_send_futex_wait_request(guest_addr, op, val, timeout, uaddr2, val3);
	fprintf(stderr, "[offload_server_futex_wait]\t[*(abi_ulong*)g2h(guest_addr) %ld ！= val %ld, sleeping...]\n", *(abi_ulong*)g2h(guest_addr), val);
	exit(222);
	pthread_mutex_lock(&futex_mutex);
	futex_uaddr_changed_flag = 0;
	while (futex_uaddr_changed_flag == 0)
	{
		pthread_cond_wait(&futex_cond, &futex_mutex);
	}
	pthread_mutex_unlock(&futex_mutex);
	fprintf(stderr, "[offload_server_futex_wait]\tawake");
	
	return 0;
}

int offload_server_futex_wake(target_ulong uaddr, int op, int val, target_ulong timeout, target_ulong uaddr2, int val3)
{
	//futex_result = 0;
	//page_recv_flag = 0;
	//offload_server_send_futex_wake_request(uaddr, op, val, timeout, uaddr2, val3);
	 
	fprintf(stderr, "[offload_server_futex_wake]\t[*(abi_ulong*)g2h(uaddr) %ld == val %ld, sleeping...]\n", *(abi_ulong*)g2h(uaddr), val);
	pthread_mutex_lock(&futex_mutex);
	futex_uaddr_changed_flag = 1;
	pthread_cond_broadcast(&futex_cond);
	pthread_mutex_unlock(&futex_mutex);

	
	return 0;
}

abi_long pass_syscall(void *cpu_env, int num, abi_long arg1,
                    abi_long arg2, abi_long arg3, abi_long arg4,
                    abi_long arg5, abi_long arg6, abi_long arg7,
                    abi_long arg8)
{
	// if ((num == TARGET_NR_futex)&&
	// 	(arg2 == FUTEX_PRIVATE_FLAG|FUTEX_WAIT) && 
	// 	(offload_server_idx > 0))// futex wait from server, ignore
	// {
	// 	fprintf(stderr, "[arm-cpu]\tI am #%ld ignoring..futex\n", offload_server_idx);
	// 	return 0;
	// 	exit(-1);
	// }
	// fprintf(stderr, "[pass_syscall]\targ2 = %ld\n",arg2);
	fprintf(stderr, "[pass_syscall]\tpassing syscall to center %ld->%ld\n", offload_server_idx, offload_thread_idx);
	// mark1 syscall time sum
	struct timeb t, tend;
    ftime(&t);
	extern void print_syscall(int num,
              abi_long arg1, abi_long arg2, abi_long arg3,
              abi_long arg4, abi_long arg5, abi_long arg6);
    if (do_strace)
		print_syscall(num,
              arg1, arg2, arg3,
              arg4, arg5, arg6);
	
	char buf[TARGET_PAGE_SIZE * 4];
	char *pp = buf + sizeof(struct tcp_msg_header);
	CPUARMState env = *((CPUARMState*)cpu_env);
	*((CPUARMState*)pp) = (CPUARMState)env;
	pp += sizeof(CPUARMState);
	fprintf(stderr, "[pass_syscall]\teabi:%lp\n",((CPUARMState *)cpu_env)->eabi);
	*((int *)pp) = (int) num;
	pp += sizeof(int);
	*((abi_long*)pp) = (abi_long)(arg1);
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)(arg2);
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)(arg3);
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)arg4;
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)arg5;
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)arg6;
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)arg7;
	pp += sizeof(abi_long);
	*((abi_long*)pp) = (abi_long)arg8;
	pp += sizeof(abi_long);
	*((int*)pp) = (int)offload_server_idx;
	pp += sizeof(int);
	*((int*)pp) = (int)offload_thread_idx;
	pp += sizeof(int);
	fprintf(stderr, "[pass_syscall]\tnum:%ld, arg1: %lp, arg2:%lp, arg3:%lp, arg4:%lp, arg5:%lp, arg6:%lp\n", num, arg1, arg2, arg3,arg4,arg5,arg6);
	struct tcp_msg_header *tcp_header = (struct tcp_msg_header *) buf;
	fill_tcp_header(tcp_header, pp - buf - sizeof(struct tcp_msg_header), TAG_OFFLOAD_SYSCALL_REQ);

	pthread_mutex_lock(&syscall_recv_mutex[offload_thread_idx]);
	int res = autoSend(client_socket, buf, pp - buf, 0);
	if (res < 0)
	{
		fprintf(stderr, "[pass_syscall]\tpassing syscall failed\n");
		exit(0);
	}
	fprintf(stderr, "[pass_syscall]\tpassed syscall, waiting...%ld->%ld\n", offload_server_idx, offload_thread_idx);
	syscall_ready_flag[offload_thread_idx] = 0;
	while (syscall_ready_flag[offload_thread_idx] == 0)
	{
		pthread_cond_wait(&syscall_recv_cond[offload_thread_idx], &syscall_recv_mutex[offload_thread_idx]);
	}
	pthread_mutex_unlock(&syscall_recv_mutex[offload_thread_idx]);
	fprintf(stderr, "[pass_syscall]\tI'm awake! %ld->%ld\n", offload_server_idx, offload_thread_idx);
	abi_long result = result_global[offload_thread_idx];
	fprintf(stderr, "[pass_syscall]\returning result %lp!\n", result);
	// calculate time diff
	ftime(&tend);
	int secDiff = tend.time - t.time;
	secDiff *= 1000;
	secDiff += (tend.millitm - t.millitm);
	syscall_time_sum += secDiff;
	fprintf(stderr, "[pass_syscall]\tbegin: %ld:%ld; end: %ld:%ld, used: %ldms, now total is: %ldms", t.time, t.millitm, tend.time, tend.millitm, secDiff, syscall_time_sum);
	fprintf(stderr, "[pass_syscall]\returning result %lp!\n", result);
	// finally crash here in server
	return result;

}

static void offload_server_process_syscall_result(void)
{
	p = net_buffer;
	abi_long result = *((abi_long *) p);
    p += sizeof(abi_long);
	int thread_id = (*(int*)p);
	p += sizeof(int);
	result_global[thread_id] = result;
	fprintf(stderr, "[offload_server_process_syscall_result]\tgot syscall ret = %lp, waking up thread%ld\n", result, thread_id);
	pthread_mutex_lock(&syscall_recv_mutex[thread_id]);
	syscall_ready_flag[thread_id] = 1;
	pthread_cond_signal(&syscall_recv_cond[thread_id]);
	pthread_mutex_unlock(&syscall_recv_mutex[thread_id]);
	//if (mutex_ready_flag == 0) offload_server_process_mutex_verified();
}

static void offload_process_tid(void)
{
	p = net_buffer;
	abi_ulong tid = *((abi_ulong*)p);	
	p += sizeof(abi_ulong);
	fprintf(stderr,"[offload_process_tid]\treceived child_tidptr: %lp\n", tid);
	return ;
	extern __thread CPUArchState *thread_env;
	if (!thread_env)
	{
		fprintf(stderr,"[offload_process_tid]\tenv: %lp\n", thread_env);
		assert(thread_env);
	}
	CPUState *cpu = ENV_GET_CPU((CPUArchState *)thread_env);
	if (!cpu)
	{
		fprintf(stderr,"[offload_process_tid]\tcpu: %lp\n", cpu);
		assert(cpu);
	}
	TaskState *ts;
	assert(cpu->opaque);
	ts = cpu->opaque;
	ts->child_tidptr = tid;
	fprintf(stderr,"[offload_process_tid]\tNOW child_tidptr: %lp\n", ts->child_tidptr);
}


static void offload_process_fork_info(void)
{
	p = net_buffer;
	unsigned int flags = *((unsigned int*)p);
	p += sizeof(unsigned int);
	abi_ulong newsp = *((abi_ulong*)p);
	p += sizeof(abi_ulong);
	abi_ulong parent_tidptr = *((abi_ulong*)p);
	p += sizeof(abi_ulong);
	target_ulong newtls = *((target_ulong*)p);
	p += sizeof(target_ulong);
	abi_ulong child_tidptr = *((abi_ulong*)p);
	p += sizeof(abi_ulong);

	fprintf(stderr,"[offload_process_fork_info]\tdoing fork local\n");
	extern int do_fork_server_local(CPUArchState *env, unsigned int flags, abi_ulong newsp,
                   abi_ulong parent_tidptr, target_ulong newtls,
                   abi_ulong child_tidptr);
	extern CPUArchState *env_bak;
	do_fork_server_local(env_bak, flags, newsp,
                    parent_tidptr, newtls,
                    child_tidptr);
	
	fprintf(stderr,"[offload_process_fork_info]\tDone.\n");

}


static void try_recv(int size)
{
	int res;
	int nleft = size;
	char* ptr = net_buffer;
	while (nleft > 0)
	{
		res = recv(client_socket, ptr, nleft, 0);
		fprintf(stderr, "[try_recv]\treceived %ld\n", res);
		if (res < 0)
		{
			printf("[try_recv]\terrno: %ld\n", res);
			perror("try_recv");
			exit(-1);
		}
		else if (res == 0)
		{
			printf( "[try_recv]\tconnection closed.\n");
			printf("[try_recv]\tnow pagefault total time = %ld, syscall total time = %ld\n", pgfault_time_sum, syscall_time_sum);
			exit(0);
		}
		else
		{
			
			nleft -= res;
			ptr += res;
			if (nleft)
				fprintf(stderr, "[try_recv]\treceived %ld B, %ld left.\n", res, nleft);
		}
		
	}
	
	return size;
}


static int autoSend(int Fd,char* buf, int length, int flag)
{
	char* ptr = buf;
	int nleft = length, res;
    pthread_mutex_lock(&server_send_mutex);
	while (nleft > 0)
	{
		fprintf(stderr, "[autoSend]\tsendding left: %ld\n", nleft);

		if ((res = send(Fd, ptr, nleft, flag)) < 0)
		{
			if (res == -1)
			{
				sleep(0.001);
				fprintf(stderr, "[autoSend]\tsend EAGAIN\n");
				perror("autoSend");
				exit(233);
				continue;
			}
			else
			{
				fprintf(stderr, "[autoSend]\tsend failed, errno: %ld\n", res);
				perror("autoSend");
				exit(0);
			}
		}
		nleft -= res;
		ptr += res;
	}
    pthread_mutex_unlock(&server_send_mutex);
	return length;
}

static void offload_server_process_page_wakeup(void)
{
	p = net_buffer;
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	fprintf(stderr, "[offload_server_process_page_wakeup]\tpage addr : %lp\n", page_addr);
	//fprintf(stderr, "[offload_server_process_fs_page]\tpage %lx perm: %s\n", page_addr, perm==1?"READ":"WRITE|READ");
	// wake up the execution thread upon this page.
	offload_page_recv_wake_up_thread(page_addr, -1);
}
static void offload_server_process_fs_page(void)
{
	//pthread_mutex_lock(&socket_mutex);
	pthread_mutex_lock(&page_process_mutex);
	if(!g_false_sharing_flag) {
		g_false_sharing_flag = 1;
	}
	p = net_buffer;
	target_ulong page_addr = *((target_ulong *) p);
    p += sizeof(target_ulong);
	abi_ulong shadow_page_addr = *((abi_ulong *) p);
	p += sizeof(abi_ulong);
	fprintf(stderr, "[offload_server_process_fs_page]\tpage addr : %lp"
					"shadow page addr : %lp\n", page_addr, shadow_page_addr);
	PageMapDesc_server *pmd = get_pmd_s(page_addr);

	assert(shadow_page_addr < 0xd0000000);
	if (offload_server_idx > 0) {
		int ret = target_mmap(shadow_page_addr, 
							MAX_PAGE_SPLIT*PAGE_SIZE, PROT_NONE,
							MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		assert(ret == shadow_page_addr);
		assert(pmd->cur_perm == 0);
		mprotect(g2h(page_addr), TARGET_PAGE_SIZE, PROT_NONE);
	}
	
	pmd->is_false_sharing = 1;
	pmd->shadow_page_addr = shadow_page_addr;
	//fprintf(stderr, "[offload_server_process_fs_page]\tpage %lx perm: %s\n", page_addr, perm==1?"READ":"WRITE|READ");
	// wake up the execution thread upon this required page.
	pthread_mutex_unlock(&page_process_mutex);
	offload_page_recv_wake_up_thread(page_addr, 0);
}
