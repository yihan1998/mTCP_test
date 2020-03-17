#include "server.h"

struct timeval end_all;

#ifdef __EVAL_FRAM__
int trans_start_flag = 0;

int accept_time = 0;
int accept_cnt = 0;

int read_time = 0;
int read_cnt = 0;
#endif

int init_ring_buff(struct ring_buf * buffer){
    buffer->buf_len = BUF_SIZE / KV_ITEM_SIZE * KV_ITEM_SIZE;
    buffer->buf_start = (char *)malloc(buffer->buf_len);
    buffer->buf_end = buffer->buf_start;
    buffer->buf_read = buffer->buf_write = 0;
    return 1;
}

int ring_buff_free(struct ring_buf * buffer){
    if(buffer->buf_read == buffer->buf_write){
        return buffer->buf_len - KV_ITEM_SIZE;
    }else{
        return buffer->buf_len - KV_ITEM_SIZE - ring_buff_used(buffer);
    }
}

int ring_buff_to_write(struct ring_buf * buffer){
    if(buffer->buf_write >= buffer->buf_read){
		if(buffer->buf_read == 0){
			return buffer->buf_len - buffer->buf_write - KV_ITEM_SIZE;
		}else{
			return buffer->buf_len - buffer->buf_write;
		}
	}else if(buffer->buf_read > buffer->buf_write){
		return buffer->buf_read - buffer->buf_write - KV_ITEM_SIZE;
	}
}

int ring_buff_used(struct ring_buf * buffer){
    if(buffer->buf_read == buffer->buf_write){
        return 0;
    }else{
        return (buffer->buf_write + buffer->buf_len - buffer->buf_read) % buffer->buf_len;
    }
}

void CleanServerVariable(struct server_vars *sv){
	sv->recv_len = 0;
	sv->request_len = 0;
	sv->total_read = 0;
	sv->total_sent = 0;
	sv->done = 0;
	sv->rspheader_sent = 0;
	sv->keep_alive = 0;
	sv->total_time = 0;
    sv->recv_buf = (struct ring_buf *)malloc(RING_BUF_SIZE);
    init_ring_buff(sv->recv_buf);
}

void CloseConnection(struct thread_context *ctx, int sockid, struct server_vars *sv){
#ifdef __REAL_TIME_STATS__
	gettimeofday(&end_all, NULL);
#endif

#ifdef __EVAL_FRAM__
	trans_start_flag = 0;
#endif

	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
	mtcp_close(ctx->mctx, sockid);
}

int HandleReadEvent(struct thread_context *ctx, int thread_id, int sockid, struct server_vars *sv){

	FILE * fp = fopen("log.txt", "a+");

	char buff[1024];
	sprintf(buff, "===== HandleReadEvent =====\n");
	fwrite(buff, strlen(buff), 1, fp);
	fflush(fp);

#ifdef __EVAL_FRAM__
    struct timeval start;
    gettimeofday(&start, NULL);
#endif
/*
	char buf[BUF_SIZE];

	int len, sent;
	
    len = mtcp_recv(ctx->mctx, sockid, buf, BUF_SIZE, 0);
	if (len <= 0) {
		return len;
	}

    sent = mtcp_write(ctx->mctx, sockid, buf, len);
*/

	int len, sent, recv_len;
	len = 0;

	struct ring_buf * recv_buf = sv->recv_buf;

//    struct kv_trans_item * recv_item = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);

	//len = mtcp_recv(ctx->mctx, sockid, (char *)(recv_buf->buf_start + recv_buf->buf_write), ring_buff_free(recv_buf), 0);
    //recv_buf->buf_write = (recv_buf->buf_write + len) % recv_buf->buf_len;

// ring buffer

	while(1){
		sprintf(buff, "[SERVER] to write len: %d, read: %d, write: %d\n", ring_buff_to_write(recv_buf), recv_buf->buf_read, recv_buf->buf_write);
		fwrite(buff, strlen(buff), 1, fp);
		fflush(fp);
		recv_len = mtcp_recv(ctx->mctx, sockid, (char *)(recv_buf->buf_start + recv_buf->buf_write), ring_buff_to_write(recv_buf), 0);
    	if(recv_len == 0 || (recv_len < 0 && errno == EAGAIN)){
			break;
		}
		len += recv_len;
		recv_buf->buf_write = (recv_buf->buf_write + recv_len) % recv_buf->buf_len;
		fwrite(buff, strlen(buff), 1, fp);
		fflush(fp);
	}

	int recv_num = len / KV_ITEM_SIZE;

	sprintf(buff, "[SERVER] recv_len: %d\n", len);
	fwrite(buff, strlen(buff), 1, fp);
	fflush(fp);

//process request
/*
    int i, res, ret;
    for(i = 0;i < recv_num;i++){
        if(recv_item[i].len > 0){
            //printf("[SERVER] put KV item\n");
            res = hi->insert(thread_id, (uint8_t *)recv_item[i].key, (uint8_t *)recv_item[i].value);
            //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item[i].key, VALUE_SIZE, recv_item[i].value);
            if (res == true){
                //printf("[SERVER] insert success\n");
				//sprintf(buff, "[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item[i].key, VALUE_SIZE, recv_item[i].value);
				//fwrite(buff, strlen(buff), 1, fp);
				//fflush(fp);
            }
        }else if(recv_item[i].len == 0){
            res = hi->search(thread_id, (uint8_t *)recv_item[i].key, (uint8_t *)recv_item[i].value);
            if(res == true){
                //printf("[SERVER] search success\n");
                recv_item[i].len = VALUE_SIZE;
				sent = mtcp_write(ctx->mctx, sockid, (char *)&recv_item[i], KV_ITEM_SIZE);
				//sprintf(buff, "[SERVER] get key: %.*s\nget value: %.*s\n", KEY_SIZE, recv_item[i].key, VALUE_SIZE, recv_item[i].value);
				//fwrite(buff, strlen(buff), 1, fp);
				//fflush(fp);
			}else{
                //printf("[SERVER] search failed\n");
                recv_item[i].len = -1;
				sent = mtcp_write(ctx->mctx, sockid, (char *)&recv_item[i], KV_ITEM_SIZE);
            }
        }
    }

	fclose(fp);
*/
/*
	sprintf(buff, "[SERVER] recv item len: %d\n", recv_item->len);
	fwrite(buff, strlen(buff), 1, fp);
	fflush(fp);
	int res, ret;
    if(recv_item->len > 0){
        //printf("[SERVER] put KV item\n");
        res = hi->insert(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
        //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item[i].key, VALUE_SIZE, recv_item[i].value);
        if (res == true){
            //printf("[SERVER] insert success\n");
			sprintf(buff, "[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
			fwrite(buff, strlen(buff), 1, fp);
			fflush(fp);
        }
    }else if(recv_item->len == 0){
        res = hi->search(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
        if(res == true){
            //printf("[SERVER] search success\n");
            recv_item->len = VALUE_SIZE;
			sent = mtcp_write(ctx->mctx, sockid, (char *)recv_item, KV_ITEM_SIZE);
			sprintf(buff, "[SERVER] get key: %.*s\nget value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
			fwrite(buff, strlen(buff), 1, fp);
			fflush(fp);
		}else{
            //printf("[SERVER] search failed\n");
            recv_item->len = -1;
			sent = mtcp_write(ctx->mctx, sockid, (char *)recv_item, KV_ITEM_SIZE);
        }
    }
*/

    int res;
    while(ring_buff_used(recv_buf) >= KV_ITEM_SIZE){
        struct kv_trans_item * recv_item = (struct kv_trans_item *)(recv_buf->buf_start + recv_buf->buf_read);
		sprintf(buff, "[SERVER] recv item len: %d\n", recv_item->len);
		fwrite(buff, strlen(buff), 1, fp);
		fflush(fp);
        if(recv_item->len > 0){
            //printf("[SERVER] put KV item\n");
            res = hi->insert(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
            //printf("[SERVER] put key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
            if (res == true){
                //printf("[SERVER] insert success\n");
				//sprintf(buff, "[SERVER] PUT success! key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
				sprintf(buff, "[SERVER] PUT success! key: %.*s\n", KEY_SIZE, recv_item->key);
				fwrite(buff, strlen(buff), 1, fp);
				fflush(fp);
            }else{
				//sprintf(buff, "[SERVER] PUT failed! key: %.*s\nput value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
				sprintf(buff, "[SERVER] PUT failed! key: %.*s\n", KEY_SIZE, recv_item->key);
				fwrite(buff, strlen(buff), 1, fp);
				fflush(fp);
			}
        }else if(recv_item->len == 0){
            res = hi->search(thread_id, (uint8_t *)recv_item->key, (uint8_t *)recv_item->value);
            //printf("[SERVER] GET key: %.*s\n value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
            if(res == true){
                //printf("[SERVER] get KV item success\n");
                recv_item->len = VALUE_SIZE;
                sent = mtcp_write(ctx->mctx, sockid, (char *)recv_item, KV_ITEM_SIZE);
				//sprintf(buff, "[SERVER] GET success! key: %.*s\nget value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
				sprintf(buff, "[SERVER] GET success! key: %.*s\n", KEY_SIZE, recv_item->key);
				fwrite(buff, strlen(buff), 1, fp);
				fflush(fp);
            }else{
                //printf("[SERVER] get KV item failed\n");
                recv_item->len = -1;
                sent = mtcp_write(ctx->mctx, sockid, (char *)recv_item, KV_ITEM_SIZE);
				//sprintf(buff, "[SERVER] GET failed! key: %.*s\nget value: %.*s\n", KEY_SIZE, recv_item->key, VALUE_SIZE, recv_item->value);
				sprintf(buff, "[SERVER] GET failed! key: %.*s\n", KEY_SIZE, recv_item->key);
				fwrite(buff, strlen(buff), 1, fp);
				fflush(fp);
            }
        }
        recv_buf->buf_read = (recv_buf->buf_read + KV_ITEM_SIZE) % recv_buf->buf_len;
    }
	
	sprintf(buff, "[SERVER] read: %d, write: %d, remain len: %d\n", recv_buf->buf_read, recv_buf->buf_write, ring_buff_used(recv_buf));
	fwrite(buff, strlen(buff), 1, fp);
	fflush(fp);

	fclose(fp);
	
#ifdef __EVAL_FRAM__
    struct timeval end;
    gettimeofday(&end, NULL);

    double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

	read_cnt++;
	read_time += (int)(end_time - start_time);
#endif

    return len;
}

int AcceptConnection(struct thread_context *ctx, int listener){
	mctx_t mctx = ctx->mctx;
	struct server_vars *sv;
	struct mtcp_epoll_event ev;
	int c;

#ifdef __EVAL_FRAM__
	struct timeval start;
	gettimeofday(&start, NULL);
#endif

	c = mtcp_accept(mctx, listener, NULL, NULL);

	if (c >= 0) {
		if (c >= MAX_FLOW_NUM) {
			TRACE_ERROR("Invalid socket id %d.\n", c);
			return -1;
		}

		sv = &ctx->svars[c];
		CleanServerVariable(sv);
		TRACE_APP("New connection %d accepted.\n", c);
		ev.events = MTCP_EPOLLIN;
		ev.data.sockid = c;
		mtcp_setsock_nonblock(ctx->mctx, c);
		mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, c, &ev);
		TRACE_APP("Socket %d registered.\n", c);

	} else {
		if (errno != EAGAIN) {
			TRACE_ERROR("mtcp_accept() error %s\n", 
					strerror(errno));
		}
	}

#ifdef __EVAL_FRAM__
	struct timeval end;
	gettimeofday(&end, NULL);

	double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
    double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

	accept_cnt++;
	accept_time += (int)(end_time - start_time);
#endif

	return c;
}

struct thread_context * InitializeServerThread(int core){
	struct thread_context *ctx;

	/* affinitize application thread to a CPU core */
#if HT_SUPPORT
	mtcp_core_affinitize(core + (num_cores / 2));
#else
	mtcp_core_affinitize(core);
#endif /* HT_SUPPORT */

	ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		TRACE_ERROR("Failed to create thread context!\n");
		return NULL;
	}

	/* create mtcp context: this will spawn an mtcp thread */
	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) {
		TRACE_ERROR("Failed to create mtcp context!\n");
		free(ctx);
		return NULL;
	}

	/* create epoll descriptor */
	ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
	if (ctx->ep < 0) {
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create epoll descriptor!\n");
		return NULL;
	}

	/* allocate memory for server variables */
	ctx->svars = (struct server_vars *)
			calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
	if (!ctx->svars) {
		mtcp_close(ctx->mctx, ctx->ep);
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create server_vars struct!\n");
		return NULL;
	}

	return ctx;
}

int CreateListeningSocket(struct thread_context *ctx){
	int listener;
	struct mtcp_epoll_event ev;
	struct sockaddr_in saddr;
	int ret;

	/* create socket and set it as nonblocking */
	listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
	if (listener < 0) {
		TRACE_ERROR("Failed to create listening socket!\n");
		return -1;
	}
	ret = mtcp_setsock_nonblock(ctx->mctx, listener);
	if (ret < 0) {
		TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
		return -1;
	}

	/* bind to port 12345 */
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(12345);
	ret = mtcp_bind(ctx->mctx, listener, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		TRACE_ERROR("Failed to bind to the listening socket!\n");
		return -1;
	}

	/* listen (backlog: can be configured) */
	ret = mtcp_listen(ctx->mctx, listener, backlog);
	if (ret < 0) {
		TRACE_ERROR("mtcp_listen() failed!\n");
		return -1;
	}
	
	/* wait for incoming accept events */
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = listener;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, listener, &ev);

	return listener;
}

void * RunServerThread(void *arg){
//	int core = *(int *)arg;
	struct server_arg * args = (struct server_arg *)arg;

	int core = args->core;
	int thread_id = args->thread_id;
//    struct hikv * hi = args->hi;
//	struct hikv_arg hikv_args = args->hikv_args;

	struct thread_context *ctx;
	mctx_t mctx;
	int listener;
	int ep;
	struct mtcp_epoll_event *events;
	int nevents;
	int i, ret;
	int do_accept;

#ifdef __REAL_TIME_STATS__
	struct timeval start;
	int start_flag = 0;

	int request_cnt = 0;
	int byte_sent = 0;
#endif

#ifdef __EVAL_FRAM__
	int cycle_cnt, handle_time, cycle_time;
	cycle_cnt = handle_time = cycle_time = 0;
#endif
	
	/* initialization */
	ctx = InitializeServerThread(core);
	if (!ctx) {
		TRACE_ERROR("Failed to initialize server thread.\n");
		return NULL;
	}
	mctx = ctx->mctx;
	ep = ctx->ep;

	events = (struct mtcp_epoll_event *)
			calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
	if (!events) {
		TRACE_ERROR("Failed to create event struct!\n");
		exit(-1);
	}

	listener = CreateListeningSocket(ctx);
	if (listener < 0) {
		TRACE_ERROR("Failed to create listening socket.\n");
		exit(-1);
	}

	while (!done[core]) {
#ifdef __EVAL_FRAM__
		struct timeval epoll_start;
		gettimeofday(&epoll_start, NULL);
#endif
		nevents = mtcp_epoll_wait(mctx, ep, events, MAX_EVENTS, -1);
		if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			break;
		}
#ifdef __EVAL_FRAM__
		struct timeval handle_start;
		gettimeofday(&handle_start, NULL);
#endif
		do_accept = FALSE;
		for (i = 0; i < nevents; i++) {

			if (events[i].data.sockid == listener) {
				/* if the event is for the listener, accept connection */
				do_accept = TRUE;

			} else if (events[i].events & MTCP_EPOLLERR) {
				int err;
				socklen_t len = sizeof(err);

				/* error on the connection */
				TRACE_APP("[CPU %d] Error on socket %d\n", 
						core, events[i].data.sockid);
				if (mtcp_getsockopt(mctx, events[i].data.sockid, 
						SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
					if (err != ETIMEDOUT) {
						fprintf(stderr, "Error on socket %d: %s\n", 
								events[i].data.sockid, strerror(err));
					}
				} else {
					perror("mtcp_getsockopt");
				}
				CloseConnection(ctx, events[i].data.sockid, 
						&ctx->svars[events[i].data.sockid]);

			} else if (events[i].events & MTCP_EPOLLIN) {
				ret = HandleReadEvent(ctx, thread_id, events[i].data.sockid, 
						&ctx->svars[events[i].data.sockid]);

#ifdef __REAL_TIME_STATS__
				request_cnt++;
				byte_sent += ret;
#endif

				if (ret == 0) {
					/* connection closed by remote host */
					CloseConnection(ctx, events[i].data.sockid, 
							&ctx->svars[events[i].data.sockid]);
				} else if (ret < 0) {
					/* if not EAGAIN, it's an error */
					if (errno != EAGAIN) {
						CloseConnection(ctx, events[i].data.sockid, 
								&ctx->svars[events[i].data.sockid]);
					}
				}

			} else {
				assert(0);
			}
		}

		/* if do_accept flag is set, accept connections */
		if (do_accept) {
			while (1) {
				ret = AcceptConnection(ctx, listener);
				if (ret < 0)
					break;
			}
#ifdef __REAL_TIME_STATS__
		    if(!start_flag){
        		gettimeofday(&start, NULL);
        		start_flag = 1;
    		}
#endif
		}
#ifdef __EVAL_FRAM__
		struct timeval end;
		gettimeofday(&end, NULL);

		if(trans_start_flag){
			double epoll_start_time = (double)epoll_start.tv_sec * 1000000 + (double)epoll_start.tv_usec;
			double handle_start_time = (double)handle_start.tv_sec * 1000000 + (double)handle_start.tv_usec;
	        double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

    	    cycle_cnt++;
	    	handle_time += (int)(end_time - handle_start_time);
	    	cycle_time += (int)(end_time - epoll_start_time);
		}

		if(do_accept){
			trans_start_flag = 1;
		}
#endif
	}

#ifdef __EVAL_FRAM__
	char buff[100];
    
    sprintf(buff, "tot_cycle %.4f tot_handle %.4f accept %.4f handleRead %.4f\n", 
			((double)cycle_time)/cycle_cnt, ((double)handle_time)/cycle_cnt, 
			((double)accept_time)/accept_cnt, ((double)read_time)/read_cnt);

    FILE * fp = fopen("cycle.txt", "a+");
    fseek(fp, 0, SEEK_END);
    
    fwrite(buff, strlen(buff), 1, fp);
    fclose(fp);	
#endif

#ifdef __REAL_TIME_STATS__

    double start_time = (double)start.tv_sec + ((double)start.tv_usec/(double)1000000);
    double end_time = (double)end_all.tv_sec + ((double)end_all.tv_usec/(double)1000000);

	double elapsed = end_time - start_time;

	FILE * fp = fopen("throughput.txt", "a+");
    fseek(fp, 0, SEEK_END);

    char buff[1024];

    sprintf(buff, "rps %.4f throughput %.4f\n", 
            ((double)request_cnt)/elapsed, ((double)byte_sent)/elapsed);
    
    fwrite(buff, strlen(buff), 1, fp);

    fclose(fp);
#endif

	/* destroy mtcp context: this will kill the mtcp thread */
	mtcp_destroy_context(mctx);
	pthread_exit(NULL);

	return NULL;
}

void SignalHandler(int signum){
	int i;

	for (i = 0; i < core_limit; i++) {
		if (app_thread[i] == pthread_self()) {
			//TRACE_INFO("Server thread %d got SIGINT\n", i);
			done[i] = TRUE;
		} else {
			if (!done[i]) {
				pthread_kill(app_thread[i], signum);
			}
		}
	}
}

int main(int argc, char **argv){
	int ret;
	struct mtcp_conf mcfg;
	int cores[MAX_CPUS];
	int process_cpu;
//	int i, o;

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	core_limit = num_cores;
	process_cpu = -1;

	char conf_name[] = "server.conf";
	conf_file = conf_name;

	if (argc < 2) {
		TRACE_CONFIG("$%s directory_to_service\n", argv[0]);
		return FALSE;
	}
#if 0
	while (-1 != (o = getopt(argc, argv, "N:f:c:b"))) {
		switch (o) {
		case 'N':
			core_limit = mystrtol(optarg, 10);
			if (core_limit > num_cores) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", num_cores);
				return FALSE;
			}
			/** 
			 * it is important that core limit is set 
			 * before mtcp_init() is called. You can
			 * not set core_limit after mtcp_init()
			 */
			mtcp_getconf(&mcfg);
			mcfg.num_cores = core_limit;
			mtcp_setconf(&mcfg);
			break;
		case 'f':
			conf_file = optarg;
			break;
		case 'c':
			process_cpu = mystrtol(optarg, 10);
			if (process_cpu > core_limit) {
				TRACE_CONFIG("Starting CPU is way off limits!\n");
				return FALSE;
			}
			break;
		case 'b':
			backlog = mystrtol(optarg, 10);
			break;
		}
	}
#endif

    int tot_test = NUM_KEYS;
    int put_percent = PUT_PERCENT;

	struct hikv_arg * hikv_args = (struct hikv_arg *)malloc(HIKV_ARG_SIZE);

	hikv_args->pm_size = 2;
	hikv_args->num_server_thread = 1;
	hikv_args->num_backend_thread = 1;
	hikv_args->num_warm_kv = 0;
	hikv_args->num_put_kv = tot_test * put_percent / 100;
	hikv_args->num_get_kv = tot_test * (100 - put_percent) / 100;
	hikv_args->num_delete_kv = 0;
	hikv_args->num_scan_kv = 0;
	hikv_args->scan_range = 100;
	hikv_args->seed = 1234;
	hikv_args->scan_all = 0;

	int i;

    for (i = 0; i < argc; i++){
        long long unsigned n;
        char junk;
        if(sscanf(argv[i], "--core_limit=%llu%c", &n, &junk) == 1){
            core_limit = n;
			if (core_limit > num_cores) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", num_cores);
				return FALSE;
			}
			mtcp_getconf(&mcfg);
			mcfg.num_cores = core_limit;
			mtcp_setconf(&mcfg);
        }else if(sscanf(argv[i], "--process_cpu=%llu%c", &n, &junk) == 1){
            process_cpu = n;
			if (process_cpu > core_limit) {
				TRACE_CONFIG("Starting CPU is way off limits!\n");
				return FALSE;
			}
        }else if(sscanf(argv[i], "--pm_size=%llu%c", &n, &junk) == 1){
            hikv_args->pm_size = n;
        }else if(sscanf(argv[i], "--num_server_thread=%llu%c", &n, &junk) == 1){
            hikv_args->num_server_thread = n;
        }else if(sscanf(argv[i], "--num_backend_thread=%llu%c", &n, &junk) == 1){
            hikv_args->num_backend_thread = n;
        }else if(sscanf(argv[i], "--num_warm=%llu%c", &n, &junk) == 1){
            hikv_args->num_warm_kv = n;
        }else if(sscanf(argv[i], "--num_test=%llu%c", &n, &junk) == 1){
            tot_test = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_args->num_put_kv = n;
        }else if(sscanf(argv[i], "--put_percent=%d%c", &put_percent, &junk) == 1){
//            hikv_thread_arg.num_get_kv = hikv_thread_arg.num_put_kv * (100 - n) / n;
//            printf("[CLIENT] [PUT]: %llu [GET]: %llu\n", hikv_thread_arg.num_put_kv, hikv_thread_arg.num_get_kv);
			hikv_args->num_put_kv = tot_test * put_percent / 100;
            hikv_args->num_get_kv = tot_test * (100 - put_percent) / 100; 
		}else if(sscanf(argv[i], "--num_get=%llu%c", &n, &junk) == 1){
            hikv_args->num_get_kv = n;
        }else if(sscanf(argv[i], "--num_delete=%llu%c", &n, &junk) == 1){
            hikv_args->num_delete_kv = n;
        }else if(sscanf(argv[i], "--num_scan=%llu%c", &n, &junk) == 1){
            hikv_args->num_scan_kv = n;
        }else if(sscanf(argv[i], "--scan_range=%llu%c", &n, &junk) == 1){
            hikv_args->scan_range = n;
        }else if(sscanf(argv[i], "--num_scan_all=%llu%c", &n, &junk) == 1){
            hikv_args->scan_all = n;
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

	size_t pm_size = hikv_args->pm_size;
    uint64_t num_server_thread = hikv_args->num_server_thread;
    uint64_t num_backend_thread = hikv_args->num_backend_thread;
    uint64_t num_warm_kv = hikv_args->num_warm_kv;
    uint64_t num_put_kv = hikv_args->num_put_kv;

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

    //Initialize Key-Value storage

	char pmem[128] = "/home/pmem0/pm";
    char pmem_meta[128] = "/home/pmem0/pmMETA";
    hi = new hikv(pm_size * 1024 * 1024 * 1024, num_server_thread, num_backend_thread, num_server_thread * (num_put_kv + num_warm_kv), pmem, pmem_meta);

	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		cores[i] = i;
		done[i] = FALSE;
		sv_thread_arg[i].core = i;
        sv_thread_arg[i].thread_id = i;
//        sv_thread_arg[i].hi = hi;
//		memcpy(&sv_thread_arg[i].hikv_args, &hikv_args, HIKV_ARG_SIZE);
		
		if (pthread_create(&app_thread[i], 
				   NULL, RunServerThread, (void *)&sv_thread_arg[i])) {
			perror("pthread_create");
			TRACE_CONFIG("Failed to create server thread.\n");
				exit(EXIT_FAILURE);
		}
		if (process_cpu != -1)
			break;
	}
	
	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		pthread_join(app_thread[i], NULL);

		if (process_cpu != -1)
			break;
	}
	
	mtcp_destroy();
	return 0;
}
