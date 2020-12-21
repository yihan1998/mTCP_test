#include "client.h"

int CreateConnection(thread_context_t ctx){
    mctx_t mctx = ctx->mctx;
	struct mtcp_epoll_event ev;
	struct sockaddr_in addr;
	int sockid;
	int ret;

	sockid = mtcp_socket(mctx, AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		TRACE_INFO("Failed to create socket!\n");
		return -1;
	}
	memset(&ctx->vars[sockid], 0, sizeof(struct client_vars));
	ret = mtcp_setsock_nonblock(mctx, sockid);
	if (ret < 0) {
		TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
		exit(-1);
	}

    ctx->vars[sockid]->file_ptr = input_file;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = daddr;
	addr.sin_port = dport;
	
	ret = mtcp_connect(mctx, sockid, 
			(struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		if (errno != EINPROGRESS) {
			perror("mtcp_connect");
			mtcp_close(mctx, sockid);
			return -1;
		}
	}

	ctx->started++;
	ctx->pending++;
	ctx->stat.connects++;

	ev.events = MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, sockid, &ev);

	return sockid;
}

static inline void 
CloseConnection(thread_context_t ctx, int sockid)
{
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
	mtcp_close(ctx->mctx, sockid);
	ctx->pending--;
	ctx->done++;
	assert(ctx->pending >= 0);
	while (ctx->pending < concurrency && ctx->started < ctx->target) {
		if (CreateConnection(ctx) < 0) {
			done[ctx->core] = TRUE;
			break;
		}
	}
}

static inline int
HandleReadEvent(thread_context_t ctx, int sockid, struct client_vars *vars)
{
    mctx_t mctx = ctx->mctx;

    char recv_buff[buff_size];
    int recv_len = 0;
    //printf(" [%s] total recv: %d, total send: %d\n", __func__, total_recv, total_send);
    //printf(" [%s] recv data(len: %d): %.*s\n", __func__, recv_len, recv_len, recv_buff);
    int len = mtcp_read(mctx, sockid, buf, buff_size);
    
}

static inline int
HandleWriteEvent(thread_context_t ctx, int sockid, struct client_vars *vars)
{
    mctx_t mctx = ctx->mctx;

    if (vars->file_ptr + buff_size >= input_file + M_512) {
        vars->file_ptr = input_file;
    }

    int send_len = mtcp_write(mctx, sockid, vars->file_ptr, buff_size);
    vars->total_send += send_len;

    vars->file_ptr = input_file + ((vars->file_ptr - input_file) + send_len) % M_512;
}

void * RunClientThread(void * arg){
    thread_context_t ctx;
	mctx_t mctx;
	int core = *(int *)arg;
	struct in_addr daddr_in;
	int n, maxevents;
	int ep;
	struct mtcp_epoll_event *events;
	int nevents;
	struct client_vars *vars;
	int i;

	struct timeval cur_tv, prev_tv;
	//uint64_t cur_ts, prev_ts;

	mtcp_core_affinitize(core);

	ctx = CreateContext(core);
	if (!ctx) {
		return NULL;
	}
	mctx = ctx->mctx;
	g_ctx[core] = ctx;
	g_stat[core] = &ctx->stat;
	srand(time(NULL));

	mtcp_init_rss(mctx, saddr, IP_RANGE, daddr, dport);

	ctx->target = client_thread_num;

	daddr_in.s_addr = daddr;
	fprintf(stderr, "Thread %d handles %d flows. connecting to %s:%u\n", 
			core, n, inet_ntoa(daddr_in), ntohs(dport));

	/* Initialization */
	maxevents = max_fds * 3;
	ep = mtcp_epoll_create(mctx, maxevents);
	if (ep < 0) {
		fprintf(stderr,"Failed to create epoll struct!n");
		exit(EXIT_FAILURE);
	}
	events = (struct mtcp_epoll_event *)
			calloc(maxevents, sizeof(struct mtcp_epoll_event));
	if (!events) {
		fprintf(stderr,"Failed to allocate events!\n");
		exit(EXIT_FAILURE);
	}
	ctx->ep = ep;

	vars = (struct client_vars *)calloc(max_fds, sizeof(struct client_vars));
	if (!vars) {
		fprintf(stderr,"Failed to create wget variables!\n");
		exit(EXIT_FAILURE);
	}
	ctx->vars = vars;

	ctx->started = ctx->done = ctx->pending = 0;
	ctx->errors = ctx->incompletes = 0;

	gettimeofday(&cur_tv, NULL);
	//prev_ts = TIMEVAL_TO_USEC(cur_tv);
	prev_tv = cur_tv;

    FILE * fp = fopen("input.dat", "rb");

    fread(input_file, 1, M_512, fp);

    fclose(fp);

	while (!done[core]) {
		gettimeofday(&cur_tv, NULL);
		//cur_ts = TIMEVAL_TO_USEC(cur_tv);

		/* print statistics every second */
		if (core == 0 && cur_tv.tv_sec > prev_tv.tv_sec) {
		  	PrintStats();
			prev_tv = cur_tv;
		}

		while (ctx->pending < concurrency && ctx->started < ctx->target) {
			if (CreateConnection(ctx) < 0) {
				done[core] = TRUE;
				break;
			}
		}

		nevents = mtcp_epoll_wait(mctx, ep, events, maxevents, -1);
		ctx->stat.waits++;
	
		if (nevents < 0) {
			if (errno != EINTR) {
				fprintf(stderr,"mtcp_epoll_wait failed! ret: %d\n", nevents);
			}
			done[core] = TRUE;
			break;
		} else {
			ctx->stat.events += nevents;
		}

		for (i = 0; i < nevents; i++) {

			if (events[i].events & MTCP_EPOLLERR) {
				int err;
				socklen_t len = sizeof(err);

				fprintf(stdout,"[CPU %d] Error on socket %d\n", 
						core, events[i].data.sockid);
				ctx->stat.errors++;
				ctx->errors++;
				if (mtcp_getsockopt(mctx, events[i].data.sockid, 
							SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
					if (err == ETIMEDOUT)
						ctx->stat.timedout++;
				}
				CloseConnection(ctx, events[i].data.sockid);

			} else if (events[i].events & MTCP_EPOLLIN) {
				HandleReadEvent(ctx, 
						events[i].data.sockid, &vars[events[i].data.sockid]);

			} else if (events[i].events == MTCP_EPOLLOUT) {
				HandleWriteEvent(ctx, 
						events[i].data.sockid, &vars[events[i].data.sockid]);

			} else {
				fprintf(stdout,"Socket %d: event: %s\n", 
						events[i].data.sockid, EventToString(events[i].events));
				assert(0);
			}
		}

		if (ctx->done >= ctx->target) {
			fprintf(stdout, "[CPU %d] Completed %d connections, "
					"errors: %d incompletes: %d\n", 
					ctx->core, ctx->done, ctx->errors, ctx->incompletes);
			break;
		}
	}

	DestroyContext(ctx);

	pthread_exit(NULL);
	return NULL;
}

void 
SignalHandler(int signum)
{
	int i;

	for (i = 0; i < core_limit; i++) {
		done[i] = TRUE;
	}
}

int main(int argc, char * argv[]){
    int ret;
	struct mtcp_conf mcfg;
	int cores[MAX_CPUS];
	int process_cpu;

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	num_core = num_cores;
	process_cpu = -1;

	char conf_name[] = "server.conf";
	conf_file = conf_name;

	if (argc < 2) {
		TRACE_CONFIG("$%s directory_to_service\n", argv[0]);
		return FALSE;
	}

    for (int i = 0; i < argc; i++){
        long long unsigned n;
        char junk;
        if(sscanf(argv[i], "--num_core=%llu%c", &n, &junk) == 1){
            num_core = n;
			printf(" >> core num: %d\n", num_core);
			if (num_core > MAX_CPUS) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", MAX_CPUS);
				return FALSE;
			}
			mtcp_getconf(&mcfg);
			mcfg.num_cores = num_core;
			mtcp_setconf(&mcfg);
        }else if(sscanf(argv[i], "--num_thread=%llu%c", &n, &junk) == 1){
            client_thread_num = n; 
			printf(" >> thread num: %d\n", client_num);
        }else if(sscanf(argv[i], "--size=%llu%c", &n, &junk) == 1){
            buff_size = n;
			printf(" >> buff size: %d\n", buff_size);
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

	/* initialize mtcp */
	if (conf_file == NULL) {
		TRACE_CONFIG("You forgot to pass the mTCP startup config file!\n");
		exit(EXIT_FAILURE);
	}

	ret = mtcp_init(conf_file);
	if (ret) {
		TRACE_CONFIG("Failed to initialize mtcp\n");
		exit(EXIT_FAILURE);
	}

	mtcp_getconf(&mcfg);
	if (backlog > mcfg.max_concurrency) {
		TRACE_CONFIG("backlog can not be set larger than CONFIG.max_concurrency\n");
		return FALSE;
	}

	/* if backlog is not specified, set it to 4K */
	if (backlog == -1) {
		backlog = 4096;
	}
	
	/* register signal handler to mtcp */
	mtcp_register_signal(SIGINT, SignalHandler);

	TRACE_INFO("Application initialization finished.\n");

	for (int i = ((process_cpu == -1) ? 0 : process_cpu); i < num_core; i++) {
		cores[i] = i;
		done[i] = FALSE;
		sv_thread_arg[i].core = i;
        sv_thread_arg[i].thread_id = i;
		
		if (pthread_create(&app_thread[i], 
				   NULL, RunClientThread, (void *)&cores[i])) {
			perror("pthread_create");
			TRACE_CONFIG("Failed to create server thread.\n");
				exit(EXIT_FAILURE);
		}
		if (process_cpu != -1)
			break;
	}
	
	for (int i = ((process_cpu == -1) ? 0 : process_cpu); i < num_core; i++) {
		pthread_join(app_thread[i], NULL);

		if (process_cpu != -1)
			break;
	}
	
	mtcp_destroy();
	return 0;
}