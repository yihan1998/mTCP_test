#include "client.h"

// Generate keys and values for client number cn
void gen_key_corpus(LL * key_corpus, int num_put, int thread_id){
	int key_i;
	LL temp;
    
    struct timeval time1;
    gettimeofday(&time1, NULL);

    srand(time1.tv_sec ^ time1.tv_usec);

	for(key_i = 0; key_i < num_put; key_i ++) {
		LL rand1 = (LL) rand();
		LL rand2 = (LL) rand();
		key_corpus[key_i] = (rand1 << 32) ^ rand2;
		if((char) key_corpus[key_i] == 0) {
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
    LL * key_corpus = (LL *)malloc(num_put_kv * sizeof(LL));
    
    gen_key_corpus(key_corpus, num_put_kv, thread_id);

#ifdef __TEST_FILE__
    char send_buf[buf_size];
    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");
#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");
#endif

#ifdef __EV_RTT__
    struct timeval record_start[250000], record_end[250000];

    int request_cnt;
    request_cnt = 0;
    
    FILE * fp = fopen("rtt.txt", "a+");
    fseek(fp, 0, SEEK_END);
#endif

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
#elif defined(__TEST_KV__)
    //printf("===== start real work ======\n");
    int i, iter, key_i, key_j;
    
    struct kv_trans_item * req_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
    struct kv_trans_item * res_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);

    struct timeval time1, time2;
    gettimeofday(&time1, NULL);
/* [Version 1.0 - seperated tasks 1]
    //PUT
    for(iter = 0;iter < 3;iter++){
        if(rand() % 100 <= PUT_PERCENT || iter < NUM_KEYS){
            snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
			req_kv->len = VALUE_SIZE;
			memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
            //printf("[CLIENT] key: %.*s\nvalue: %.*s\n", KEY_SIZE, req_kv->key, VALUE_SIZE, req_kv->value);
            printf("[CLIENT] key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
			key_i = (key_i + 1) & NUM_KEYS_;
		}else{
			key_i = rand() & NUM_KEYS_;
			req_kv->key[0] = key_corpus[key_i];
			req_kv->len = 0;
			memset((char *)req_kv->value, 0, VALUE_SIZE);
		}

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

    //GET

        int temp = 0;
        
        struct kv_trans_item * recv_item = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);

	    int recv_size;
        
        while(1){
            recv_size = read(fd, recv_item, KV_ITEM_SIZE);
            
            printf("[CLIENT] recv len: %d\n", recv_size);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            temp += recv_size;

            if(temp == KV_ITEM_SIZE){
                printf("[CLIENT] reply key: %llu, value: %.*s\n", str_to_ll((char *)req_kv->key, KEY_SIZE), KEY_SIZE, VALUE_SIZE, req_kv->value);
                break;
            }
        }

        gettimeofday(&time2, NULL);
        if(time2.tv_sec - time1.tv_sec > 10){
            printf("[CLIENT] request complete\n");
            break;
        }
    }
*/
    uint64_t num_kv = num_get_kv + num_put_kv + num_delete_kv + num_scan_kv;
    uint64_t get_count = 0;
    uint64_t put_count = 0;
    uint64_t delete_count = 0;
    uint64_t scan_count = 0;
    uint64_t scan_kv_count = 0;
    uint64_t scan_all_count = 0;
    uint64_t seq_count = 0;
    uint64_t real_get_count = 0;

    uint64_t match_search = 0;
    uint64_t match_insert = 0;
    uint64_t match_delete = 0;

/*[Version 2.0 - seperated tasks 2]
    //PUT
    for(iter = 0;iter < num_put_kv;iter++){
        memset((char *)req_kv->key, 0, KEY_SIZE);
        memset((char *)req_kv->value, 0, VALUE_SIZE);
        snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
		req_kv->len = VALUE_SIZE;
		memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
        printf("[CLIENT] key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
		key_i = (key_i + 1) & NUM_KEYS_;

        put_count++;

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}else{
            match_insert++;
        }
    }

    //GET
    
    for(iter = 0, key_i = 0;iter < num_get_kv;iter++){
        snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
		req_kv->len = 0;
		memset((char *)req_kv->value, 0, VALUE_SIZE);

        if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
			perror("[CLIENT] send failed");
	    	exit(1);
    	}

        get_count++;

        int tot_recv = 0;

	    int recv_size;

        while(1){
            recv_size = read(fd, res_kv, KV_ITEM_SIZE);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            tot_recv += recv_size;

            if(tot_recv == KV_ITEM_SIZE){

                if(res_kv->len == VALUE_SIZE){
                    printf("[CLIENT] GET success! key: %.*s, value: %.*s\n", KEY_SIZE, res_kv->key, VALUE_SIZE, res_kv->value);
                    match_search++;
                }else{
                    printf("[CLIENT] GET failed! key: %.*s, value: %.*s\n", KEY_SIZE, res_kv->key, VALUE_SIZE, res_kv->value);
                }
                break;
            }
        }
        key_i = (key_i + 1) & NUM_KEYS_;
    }
*/

#ifdef __EV_RTT__
    long * rtt_time = (long *)malloc(sizeof(long) * NUM_KEYS / 4);
    struct timeval get_start, get_end;

    int request_cnt;
    request_cnt = 0;
    
    FILE * fp = fopen("rtt.txt", "a+");
    fseek(fp, 0, SEEK_END);
#endif
#ifndef BATCHED_KEY
//[Version 3.0 - mixed tests]
    for(iter = 0, key_i = 0, key_j = 0;iter < num_kv;iter++){
        if(iter < num_put_kv) {
        //PUT
            //printf(">> PUT request\n");
            struct kv_trans_item * req_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
            //printf("[CLIENT] put KV item %d\n", iter);
            snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
            //printf("[CLIENT] PUT copy value\n");
            //printf(">> value_corpus: %p, value: %.*s\n", &value_corpus[key_i * VALUE_SIZE], VALUE_SIZE, &value_corpus[key_i * VALUE_SIZE]);
    		memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
            //printf(">> req_kv->value: %p, value: %.*s\n", req_kv->value, VALUE_SIZE, req_kv->value);
    		//printf("[CLIENT] PUT key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
            //printf("[CLIENT] PUT key: %llu\n", key_corpus[key_i]);
		    key_i = (key_i + 1) % num_put_kv;

            put_count++;

        #ifdef __EV_RTT__
            gettimeofday(&record_start[request_cnt], NULL);
        #endif

            if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
	    		perror("[CLIENT] send failed");
	        	exit(1);
        	}

            //printf("[CLIENT] send success\n");

            int recv_size, tot_recv;

	        tot_recv = 0;

            char * reply = (char *)malloc(REPLY_SIZE);
            memset(reply, 0, REPLY_SIZE);

            recv_size = read(fd, reply, REPLY_SIZE);

            #ifdef __EV_RTT__
                gettimeofday(&record_end[request_cnt], NULL);
                request_cnt++;
            #endif

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            if(strcmp("put success", reply) == 0){
                //printf("put success\n");
                match_insert++;
            }else if(strcmp("put failed", reply) == 0){
                //printf("put failed\n");
            }else{
                //printf("unknown result\n");
            }

            free(req_kv);
            //printf(">> PUT end\n");
		} else {
		//GET
            //printf("[CLIENT] get KV item\n");
            char * key = (char *)malloc(KEY_SIZE);
            snprintf(key, KEY_SIZE + 1, "%0llu", key_corpus[key_j]);     //set Key

        #ifdef __EV_RTT__
            gettimeofday(&record_start[request_cnt], NULL);
        #endif

            if(write(fd, key, KEY_SIZE) < 0){
	    		perror("[CLIENT] send failed");
	        	exit(1);
    	    }

            get_count++;

            int recv_size;

            char * value = (char *)malloc(VALUE_SIZE);

	        recv_size = read(fd, value, VALUE_SIZE);

            #ifdef __EV_RTT__
                gettimeofday(&record_end[request_cnt], NULL);
                request_cnt++;
        #endif

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            if(strcmp("get failed", value) == 0){
                //printf("put failed\n");
            }else{
                //printf("[CLIENT] GET key: %.*s, value: %.*s\n", KEY_SIZE, key, VALUE_SIZE, value);
                if(bufcmp(value, (char *)&value_corpus[key_j * VALUE_SIZE], VALUE_SIZE)){
                    //printf("[CLIENT] GET success! key: %.*s, value: %.*s\n", KEY_SIZE, req_kv->key, VALUE_SIZE, req_kv->value);
                    //printf("[CLIENT] GET success! key: %.*s\n", KEY_SIZE, req_kv->key);
                    match_search++;
                }
            }

            key_j = (key_j + 1) % num_put_kv;

            free(key);
            free(value);
		}
    }

#else

//[Version 4.0 - 256B batched key] 
    for(iter = 0, key_i = 0, key_j = 0;iter < num_kv;){
        if(iter < num_put_kv) {
        //PUT
            //printf(">> PUT request\n");
            struct kv_trans_item * req_kv = (struct kv_trans_item *)malloc(KV_ITEM_SIZE);
            //printf("[CLIENT] put KV item %d\n", iter);
            snprintf((char *)req_kv->key, KEY_SIZE + 1, "%0llu", key_corpus[key_i]);     //set Key
            //printf("[CLIENT] PUT copy value\n");
            //printf(">> value_corpus: %p, value: %.*s\n", &value_corpus[key_i * VALUE_SIZE], VALUE_SIZE, &value_corpus[key_i * VALUE_SIZE]);
    		memcpy((char *)req_kv->value, (char *)&value_corpus[key_i * VALUE_SIZE], VALUE_SIZE);   //set Value
            //printf(">> req_kv->value: %p, value: %.*s\n", req_kv->value, VALUE_SIZE, req_kv->value);
    		//printf("[CLIENT] PUT key: %llu, value: %.*s\n", key_corpus[key_i], VALUE_SIZE, req_kv->value);
            //printf("[CLIENT] PUT key: %llu\n", key_corpus[key_i]);
		    key_i = (key_i + 1) % num_put_kv;

            put_count++;

            if(write(fd, req_kv, KV_ITEM_SIZE) < 0){
	    		perror("[CLIENT] send failed");
	        	exit(1);
        	}

            //printf("[CLIENT] send success\n");

            int recv_size, tot_recv;

	        tot_recv = 0;

            char * reply = (char *)malloc(REPLY_SIZE);
            memset(reply, 0, REPLY_SIZE);

            recv_size = read(fd, reply, REPLY_SIZE);

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            if(strcmp("put success", reply) == 0){
                printf("put success\n");
                match_insert++;
            }else if(strcmp("put failed", reply) == 0){
                printf("put failed\n");
            }else{
                //printf("unknown result\n");
            }

            free(req_kv);

            iter++;
		} else {
		//GET
            char * key = (char *)malloc(NUM_BATCH * KEY_SIZE);

            int send_num;
            for(send_num = 0; (key_j + send_num < num_get_kv) && (send_num < NUM_BATCH);send_num++){
                snprintf(key + send_num * KEY_SIZE, KEY_SIZE + 1, "%0llu", key_corpus[key_j + send_num]);     //set Key
            }

        #ifdef __EV_RTT__
            gettimeofday(&get_start, NULL);
        #endif

            if(write(fd, key, send_num * KEY_SIZE) < 0){
	    		perror("[CLIENT] send failed");
	        	exit(1);
    	    }

            get_count += send_num;

            int recv_size;

            char * value = (char *)malloc(send_num * VALUE_SIZE + 20);

	        recv_size = read(fd, value, send_num * VALUE_SIZE + 20);

            #ifdef __EV_RTT__
                gettimeofday(&get_end, NULL);
                long start_time = (long)get_start.tv_sec * 1000000 + (long)get_start.tv_usec;
                long end_time = (long)get_end.tv_sec * 1000000 + (long)get_end.tv_usec;
                rtt_time[request_cnt] = end_time - start_time;
                request_cnt++;
            #endif

            if(recv_size == 0){
                printf("[CLIENT] close connection\n");
                close(fd);
            }

            int recv_num = recv_size / VALUE_SIZE;

            int i;
            for(i = 0;i < recv_num;i++){
                printf("[CLIENT] value: %.*s\n", VALUE_SIZE, value + i * VALUE_SIZE);
                if(strcmp("get failed", value + i * VALUE_SIZE) == 0){
                    //printf(" >> GET failed\n");
                }else if(bufcmp(value + i * VALUE_SIZE, (char *)value_corpus + (key_j + i) * VALUE_SIZE, VALUE_SIZE)){
                    //printf("[CLIENT] GET success! key: %.*s, value: %.*s\n", KEY_SIZE, req_kv->key, VALUE_SIZE, req_kv->value);
                    //printf("[CLIENT] GET success! key: %.*s\n", KEY_SIZE, req_kv->key);
                    //printf(" >> GET success\n");
                    match_search++;
                }
            }

            key_j = (key_j + send_num) % num_get_kv;
            iter += send_num;

            free(key);
            free(value);
		}
    }
#endif

#ifdef __EV_RTT__
    int j;
    for(j = 0;j < request_cnt;j++){
        char buff[1024];

        sprintf(buff, "%ld\n", rtt_time[j]);
        
        pthread_mutex_lock(&rtt_lock);

        fwrite(buff, strlen(buff), 1, fp);
        fflush(fp);

        pthread_mutex_unlock(&rtt_lock);
    }

    fclose(fp);

#endif

    close(fd);

    printf(">>[TEST] test end\n");
    if (put_count > 0){
        printf("  [Result]insert match:%llu/%llu(%.2f%%)\n", match_insert, put_count, 100.0 * match_insert / put_count);
    }
    if (get_count > 0){
        printf("  [Result]search match:%llu/%llu(%.2f%%)\n", match_search, get_count, 100.0 * match_search / get_count);
    }
    if (delete_count > 0){
        printf("  [Result]delete match:%llu/%llu(%.2f%%)\n", match_delete, delete_count, 100.0 * match_delete / delete_count);
    }
    if (scan_count > 0){
        printf("  [Result]scan match:%llu/%llu/%llu\n", scan_kv_count, scan_count, scan_kv_count / scan_count);
    }
#endif

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
    int tot_test = NUM_KEYS;
    int put_percent = PUT_PERCENT;

    struct hikv_arg hikv_thread_arg = {
        20,                                      //pm_size
        1,                                      //num_server_thread
        1,                                      //num_backend_thread
        0,                                      //num_warm_kv
        tot_test * put_percent / 100,           //num_put_kv
        tot_test * (100 - put_percent) / 100,   //num_get_kv
        0,                                      //num_delete_kv
        0,                                      //num_scan_kv
        100,                                    //scan_range
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
        }else if(sscanf(argv[i], "--num_test=%llu%c", &n, &junk) == 1){
            tot_test = n;
        }else if(sscanf(argv[i], "--num_put=%llu%c", &n, &junk) == 1){
            hikv_thread_arg.num_put_kv = n;
        }else if(sscanf(argv[i], "--put_percent=%d%c", &n, &junk) == 1){
//            hikv_thread_arg.num_get_kv = hikv_thread_arg.num_put_kv * (100 - n) / n;
//            printf("[CLIENT] [PUT]: %llu [GET]: %llu\n", hikv_thread_arg.num_put_kv, hikv_thread_arg.num_get_kv);
            hikv_thread_arg.num_put_kv = tot_test * put_percent / 100;
            hikv_thread_arg.num_get_kv = tot_test * (100 - put_percent) / 100;
//            printf("[CLIENT] [PUT]: %llu [GET]: %llu\n", hikv_thread_arg.num_put_kv, hikv_thread_arg.num_get_kv);
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
        }else if(i > 0){
            printf("error (%s)!\n", argv[i]);
        }
    }

    value_corpus = (uint8_t *)malloc(hikv_thread_arg.num_put_kv * VALUE_SIZE);
    gen_value_corpus(value_corpus, hikv_thread_arg.num_put_kv);

    for(i = 0;i < client_thread_num;i++){
        cl_thread_arg[i].thread_id = i;
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