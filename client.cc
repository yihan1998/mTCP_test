#include "client.h"

// Generate keys and values for client number cn
void gen_key_corpus(LL * key_corpus, int num_put, int thread_id){
	int key_i;
	LL temp;
    
    struct timeval time1;
    gettimeofday(&time1, NULL);

    srand(time1.tv_sec ^ time1.tv_usec);
/*
    //random key
	for(key_i = 0; key_i < num_put; key_i ++) {
		LL rand1 = (LL) rand();
		LL rand2 = (LL) rand();
		key_corpus[key_i] = (rand1 << 32) ^ rand2;
		if((char) key_corpus[key_i] == 0) {
			key_i --;
		}
	}
*/

    //sequence key
    LL seed;
    do{
        LL rand1 = (LL) rand();
    	LL rand2 = (LL) rand();
        seed = ((rand1 << 32) ^ rand2) & 0xfffc0000;
    }while(seed == 0);
    
    for(key_i = 0; key_i < num_put; key_i ++) {
		key_corpus[key_i] = seed + key_i;
		if(key_corpus[key_i] == 0) {
			key_i --;
		}
	}

    return;
}

void gen_value_corpus(uint8_t * value_corpus, int num_put){

    FILE * fp = fopen("client-input.dat", "rb");
    fread(value_corpus, 1, num_put * VALUE_SIZE, fp);
    fclose(fp);

    return;
}

int bufcmp(char * a, char * b, int buf_len){
    int i;
    for(i = 0;i < buf_len;i++){
        if(a[i] != b[i]){
            printf("[bufcmp] diff bitween %c and %c, i = %d\n", a[i], b[i], i);
            break;
        }
    }

    return (i == buf_len)? 1: 0;
}

int connect_server(char * server_ip, int port){
    int sockfd;
    
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("[CLIENT] create socket failed");
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[CLIENT] connect server failed");
        return -1;
    }

    //evutil_make_socket_nonblocking(sockfd);

    return sockfd;

}

void * send_request(void * arg){
//    printf(">> start sending request\n");
    
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);
    struct hikv_arg * hikv_args = info->hikv_thread_arg;
    int thread_id = info->thread_id;

    size_t pm_size = hikv_args->pm_size;
    uint64_t num_server_thread = hikv_args->num_server_thread;
    uint64_t num_backend_thread = hikv_args->num_backend_thread;
    uint64_t num_warm_kv = hikv_args->num_warm_kv;
    uint64_t num_put_kv = hikv_args->num_put_kv;
    uint64_t num_get_kv = hikv_args->num_get_kv;
    uint64_t num_delete_kv = hikv_args->num_delete_kv;
    uint64_t num_scan_kv = hikv_args->num_scan_kv;
    uint64_t scan_range = hikv_args->scan_range;

    uint64_t seed = hikv_args->seed;

    //initial Key
    //printf(" >> generate key...\n");

    LL * key_corpus = (LL *)malloc(num_put_kv * sizeof(LL));
    
    gen_key_corpus(key_corpus, num_put_kv, thread_id);
    
    //printf(" >> generate key complete\n");

    char send_buf[buf_size];
    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");

    struct timeval time1, time2;
    gettimeofday(&time1, NULL);

    while(!feof(send_fp)){

#ifdef __EV_RTT__
        gettimeofday(&record_start[request_cnt], NULL);
#endif

        //send request
        send_size = fread(send_buf, 1, buf_size, send_fp);

        if(write(fd, send_buf, send_size) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

        //receive reply
        int temp = 0;
        while(1){
            recv_size = read(fd, recv_buf, buf_size);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

#ifdef RECEIVE_DEBUG
            fwrite(recv_buf, recv_size, 1, recv_fp);
            fflush(recv_fp);
#endif
            temp += recv_size;

            if(temp == send_size){
                break;
            }
        }

#ifdef __EV_RTT__
        gettimeofday(&record_end[request_cnt], NULL);

        if(record_end[request_cnt].tv_sec - record_start[0].tv_sec > 10){
            printf("[CLIENT] request complete\n");
            break;
        }

        request_cnt++;
#else
        struct timeval end;
        gettimeofday(&end, NULL);
        
        if(end.tv_sec - time1.tv_sec > 10){
            gettimeofday(&time2, NULL);
            printf("[CLIENT] request complete\n");
            break;
        }
#endif
    }

#ifdef __EV_RTT__
    int j;
    for(j = 0;j <= request_cnt;j++){
        long start_time = (long)record_start[j].tv_sec * 1000000 + (long)record_start[j].tv_usec;
        long end_time = (long)record_end[j].tv_sec * 1000000 + (long)record_end[j].tv_usec;

        char buff[1024];

        sprintf(buff, "%ld\n", end_time - start_time);
        
        pthread_mutex_lock(&rtt_lock);

        fwrite(buff, strlen(buff), 1, fp);
        fflush(fp);

        pthread_mutex_unlock(&rtt_lock);
    }

    fclose(fp);

#endif

    fclose(send_fp);

    return NULL;
}

#if 0
void response_process(int sock, short event, void * arg){
#ifdef RECEIVE_DEBUG
    struct debug_response_arg * debug_arg = (struct debug_response_arg *)arg;

    struct event * read_ev = debug_arg->read_ev;
    struct send_info * info = debug_arg->info;
    FILE * fp = debug_arg->fp;
#else
    struct response_arg * response_process_arg = (struct response_arg *)arg;

    struct event * read_ev = response_process_arg->read_ev;
    struct send_info * info = response_process_arg->info;
#endif

    int * recv_byte = info->recv_byte;
    int * send_byte = info->send_byte;

    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

    int recv_size = read(sock, recv_buf, buf_size);

    if(recv_size == 0){
        printf("[CLIENT] close connection\n");
        close(sock);
    }

#ifdef RECEIVE_DEBUG
    fwrite(recv_buf, recv_size, 1, fp);
    fflush(fp);
#endif

    (*recv_byte) += recv_size;
    
//    printf("[CLIENT %d] receive reply: %s\n", sock, recv_buf);

    if((*recv_byte) == (*send_byte)){
        printf("[CLIENT %d] receive reply complete, close connection\n", sock);

        work_done_flag = 1;

        event_del(read_ev);

#ifdef RECEIVE_DEBUG
        fclose(fp);
#endif
        close(sock);
    }
}

void * create_response_process(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    struct event_base * base = event_base_new();

    struct event * read_ev = (struct event *)malloc(sizeof(struct event));

#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");

    struct debug_response_arg * debug_arg = (struct debug_response_arg *)malloc(sizeof(struct debug_response_arg));
    debug_arg->read_ev = read_ev;
    debug_arg->info = info;
    debug_arg->fp = recv_fp;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, debug_arg);
#else
    struct response_arg * response_process_arg = (struct response_arg *)malloc(RESPONSE_ARG_SIZE);
    response_process_arg->read_ev = read_ev;
    response_process_arg->info = info;

    event_set(read_ev, fd, EV_READ|EV_PERSIST, response_process, response_process_arg);
#endif

    event_base_set(base, read_ev);

    event_add(read_ev, NULL);

    event_base_dispatch(base);
}

void receive_response_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, create_response_process, (void *)info);
    pthread_detach(thread);
}

void send_request_thread(struct send_info * info){
    pthread_t thread;
    pthread_create(&thread, NULL, send_request, (void *)info);
    pthread_detach(thread);
}
#endif
void * client_thread(void * argv){
    cpu_set_t core_set;

    CPU_ZERO(&core_set);
    CPU_SET(0, &core_set);

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

    struct client_arg * server = (struct client_arg *)argv;

//    buf_size = server->buf_size;
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;

#ifdef __EV_RTT__
    pthread_mutex_init(&rtt_lock, NULL);
#endif

    int sockfd = connect_server(server->ip_addr, server->port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
    info->send_byte = &send_byte;
    info->recv_byte = &recv_byte;
    info->hikv_thread_arg = &server->hikv_thread_arg;
    info->thread_id = server->thread_id;

    send_request(info);

//    while(!work_done_flag);

    free(info);

    return NULL;
}

int main(int argc, char * argv[]){
    int put_test, get_test, scan_test, scan_range;
    scan_range = 4;
    put_test = get_test = scan_test = NUM_KEYS;

    struct hikv_arg hikv_thread_arg = {
        20,                                      //pm_size
        1,                                      //num_server_thread
        1,                                      //num_backend_thread
        0,                                      //num_warm_kv
        put_test,                               //num_put_kv
        get_test,                               //num_get_kv
        0,                                      //num_delete_kv
        scan_test,                              //num_scan_kv
        scan_range,                             //scan_range
        1234,                                   //seed
        0                                       //scan_all
    };

    int i;

    char server_ip[20];
    int server_port;

    for (i = 0; i < argc; i++){
        double d;
        uint64_t n;
        char junk;
        if(sscanf(argv[i], "--num_thread=%llu%c", &n, &junk) == 1){
            client_thread_num = n;
        }else if(sscanf(argv[i], "--num_warm=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_warm_kv = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_put_kv = n;
        }else if(sscanf(argv[i], "--num_get=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_get_kv = n;
        }else if(sscanf(argv[i], "--num_delete=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_delete_kv = n;
        }else if(sscanf(argv[i], "--num_scan=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_scan_kv = n;
        }else if(sscanf(argv[i], "--scan_range=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.scan_range = n;
        }else if(sscanf(argv[i], "--num_scan_all=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.scan_all = n;
        }else if(sscanf(argv[i], "--server_ip=%s%c", server_ip, &junk) == 1){
            printf("[CLIENT] server ip: %s\n", server_ip);
        }else if(sscanf(argv[i], "--server_port=%d%c", &server_port, &junk) == 1){
            printf("[CLIENT] server port: %d\n", server_port);
        }else if(sscanf(argv[i], "--buf_size=%d%c", &n, &junk) == 1){
            buf_size = n;
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

    //printf(" >> generate value...\n");

    value_corpus = (uint8_t *)malloc(hikv_thread_arg.num_put_kv * VALUE_SIZE);
    gen_value_corpus(value_corpus, hikv_thread_arg.num_put_kv);

    //printf(" >> generate value complete\n");

    for(i = 0;i < client_thread_num;i++){
        cl_thread_arg[i].thread_id = i + 1;
        cl_thread_arg[i].ip_addr = server_ip;
        cl_thread_arg[i].port = server_port;
//        arg.buf_size = atoi(argv[4]);
#ifdef __BIND_CORE__
        arg.sequence = i;
#endif
        memcpy(&cl_thread_arg[i].hikv_thread_arg, &hikv_thread_arg, HIKV_ARG_SIZE);
        pthread_create(&cl_thread[i], NULL, client_thread, (void *)&cl_thread_arg[i]);
    }

    for(i = 0;i < client_thread_num;i++){
        pthread_join(cl_thread[i], NULL);
    }
}