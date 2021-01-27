#include "common.h"

#define NUM_TEST    M_1

#define TEST_FUNCTION   0
#define TEST_FILE       1

#define CPU_NUM     16

#define BIND_CORE

long long * rtt_buff;
int rtt_buff_len;

pthread_t client_thread;
struct client_arg client_thread_arg;

int num_client;
int core_id;
int num_server_core;

char input_file[M_512];
char * file_ptr;

int concurrency = 10000;
int num_connection = 0;

int eval_time;

#define MAX_FD  (concurrency * 3)

int done = 0;

int num_complete = 0;

struct timeval start;
struct timeval end;

struct param * vars;

int timerfd;

FILE * rtt_file;

int
handle_signal(int signal) {
    if (signal == SIGINT) {
        printf(" [%s] received interrupt signal", __func__);
        exit(1);
    }
}

int 
connect_server(int epfd, char * server_ip, int port) {
    //printf(" ========== [%s] ==========\n", __func__);
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

    int err = -1;
    int snd_size = 0; 
    int rcv_size = 0;  
    socklen_t optlen; 
 #if 0
    /* Set send buffer size */  
    snd_size = 65536; 
    optlen = sizeof(snd_size); 
    err = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen); 
    if(err < 0){ 
        printf(" >> Failed to set send buffer size\n"); 
    } 
 
    /* Set receive buffer size */ 
    rcv_size = 65536;    
    optlen = sizeof(rcv_size); 
    err = setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF, (char *)&rcv_size, optlen); 
    if(err < 0){ 
        printf(" >> Failed to set recv buffer size\n"); 
    } 
#endif
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[CLIENT] connect server failed");
        return -1;
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    vars[num_connection].sockfd = sockfd;
    vars[num_connection].epfd = epfd;
    vars[num_connection].type = SOCK_VAR;
    
    struct sock_info * info = (struct sock_info *)calloc(1, SOCK_INFO_SIZE);
    info->file_ptr = input_file;
    info->complete = 0;
    info->total_send = 0;
    info->total_recv = 0;

    vars[num_connection].data = (void *)info;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = &vars[num_connection];
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    gettimeofday(&start, NULL);

    num_connection++;

    return sockfd;
}

int 
close_connection(int epfd, int sockfd, struct param * vars) {
    //printf(" ========== [%s] ==========\n", __func__);
    //epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
    //printf("\t >> closing sock %d\n", sockfd);
    //shutdown(sockfd, SHUT_WR);
    //shutdown(sockfd, SHUT_RD);
    close(sockfd);

    struct sock_info * info = (struct sock_info *)vars->data;
    info->complete = 1;

    num_complete++;

    return 1;
}

int
handle_read_event(int epfd, int sockfd, struct param * vars) {
    struct sock_info * info = (struct sock_info *)vars->data;

    char recv_buff[buff_size];
    int recv_len = 0;
    //printf(" [%s] total recv: %d, total send: %d\n", __func__, total_recv, total_send);
    //printf(" [%s] recv data(len: %d): %.*s\n", __func__, recv_len, recv_len, recv_buff);
    int len = read(sockfd, recv_buff + recv_len, buff_size - recv_len);
    if (len < 0) {
        if (errno != EAGAIN) {
            perror(" >> sock recv error!");
        }
    } else {
        recv_len += len;
        info->total_recv += len;
    }

#ifdef EVAL_RTT
    struct timeval current;
    gettimeofday(&current, NULL);
    
    long long elapsed;
    if (current.tv_usec < info->start.tv_usec) {
        elapsed = 1000000 + current.tv_usec - info->start.tv_usec;
    } else {
        elapsed = current.tv_usec - info->start.tv_usec;
    }

    if (rtt_buff_len < M_128) {
        rtt_buff[rtt_buff_len++] = elapsed;
    }
#endif

    if (info->total_recv == info->total_send) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.ptr = vars;

        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
    }

}

int
handle_write_event(int epfd, int sockfd, struct param * vars) {
    struct sock_info * info = (struct sock_info *)vars->data;

    char send_buff[buff_size];
    if (info->file_ptr + buff_size >= input_file + M_512) {
        info->file_ptr = input_file;
    }
    
    memcpy(send_buff, info->file_ptr, buff_size);

#ifdef EVAL_RTT
    gettimeofday(&info->start, NULL);
#endif

    int send_len = tardis_write(sockfd, send_buff, buff_size);

    if(send_len < 0) {
        return;
    }

    info->total_send += send_len;

    info->file_ptr = input_file + ((info->file_ptr - input_file) + send_len) % M_512;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = vars;

    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);

    //printf(" [%s] send data(len: %d): %.*s\n", __func__, send_len, send_len, send_buff);
}

int
handle_timeup_event(int epfd, int timerfd) {

    for (int i = 0; i < num_connection; i++) {
        struct param * var = &vars[i];
        shutdown(var->sockfd, SHUT_WR);
    }

    sleep(2);

    for (int i = 0; i < num_connection; i++) {
        struct param * var = &vars[i];
        shutdown(var->sockfd, SHUT_RD);
    }

    sleep(2);
    
    while (num_complete < concurrency && num_complete < num_connection) {
        for (int i = 0; i < num_connection; i++) {
            struct param * var = &vars[i];
            struct sock_info * info = (struct sock_info *)var->data;
            if (!info->complete) {
                int ret = close_connection(epfd, var->sockfd, var);
                if (ret < 0) {
                    continue;
                }
                //printf(" >> Connection(sock: %d) closed, total: %d\n", var->sockfd, num_complete);
                gettimeofday(&end, NULL);
            }
        }
    }

    for (int i = 0; i < rtt_buff_len; i++) {
        fprintf(rtt_file, "%llu\n", rtt_buff[i]);
        fflush(rtt_file);
    }

    fclose(rtt_file);
    
}

void * 
RunClientThread(void * argv) {
    //printf(" ========== Running Client Thread ==========\n");

    struct client_arg * thread_arg = (struct client_arg *)argv;
    
    int core_id = thread_arg->core_id;
    char * server_ip = thread_arg->ip_addr;
    int server_port = thread_arg->port;

    cpu_set_t core_set;

    CPU_ZERO(&core_set);
#ifdef BIND_CORE
    CPU_SET(core_id, &core_set);
#else
    CPU_SET(0, &core_set);
#endif

    if (pthread_setaffinity_np(pthread_self(), sizeof(core_set), &core_set) == -1){
        printf("warning: could not set CPU affinity, continuing...\n");
    }

#if 0
    int sockfd = connect_server(server_ip, server_port);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }
    printf(" ========== Socket %d Sending Request to Server ==========\n", sockfd);

#if TEST_FUNCTION
    for (int i = 0; i < 10; i++)
    {
        /* code */
        char request[BUFF_SIZE];
        sprintf(request, "Hello Doctor %d", i);
        write(sockfd, request, BUFF_SIZE);

        char reply[BUFF_SIZE + 1];
        memset(reply, 0, BUFF_SIZE);
        read(sockfd, reply, BUFF_SIZE);
        printf(" >> receive message: %s\n", reply);
    }
#elif TEST_FILE
    char buff[BUFF_SIZE], send_buff[BUFF_SIZE];
    char * file_ptr = input_file;
    
    struct timeval start;
    gettimeofday(&start, NULL);

    for (int i = 0; i < NUM_TEST && file_ptr <= input_file + M_512; i++)
    {
        struct timeval current;
        gettimeofday(&current, NULL);

        if (current.tv_sec - start.tv_sec >= 10) {
            break;
        }
        
        memset(buff, 0, BUFF_SIZE);
        memset(send_buff, 0, BUFF_SIZE);

        memcpy(send_buff, file_ptr, BUFF_SIZE);

        int send_len = write(sockfd, send_buff, BUFF_SIZE);

        //printf(" >> send request\n");
        file_ptr += send_len;

        int recv_len = 0;
        while(recv_len < send_len) {
            int len = read(sockfd, buff + recv_len, send_len - recv_len);
            recv_len += len;
        }
        
        //printf(" >> receive reply\n");
    }

    printf(" ========== Socket %d Transmission Finished ==========\n", sockfd);
#endif
    close(sockfd);

    sleep(2);
#endif

    vars = (struct param *)calloc(MAX_FD, PARAM_SIZE);

    int epfd;
    struct epoll_event * events;
    int nevents;

    int maxevent = MAX_FD * 3;

    epfd = epoll_create1(0);

    events = (struct epoll_event *)calloc(maxevent, sizeof(struct epoll_event));

    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        printf(" >> timerfd_create error!\n");
        exit(1);
    }

    //printf("\t >> Create timerfd: %d\n", timerfd);

    struct itimerspec ts;
    
    struct param * timer_param = (struct param *)calloc(1, PARAM_SIZE);
    timer_param->sockfd = timerfd;
    timer_param->epfd = epfd;
    timer_param->type = TIMER_VAR;
    timer_param->data = NULL;

    ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;
	ts.it_value.tv_sec = eval_time;
	ts.it_value.tv_nsec = 0;

	if (timerfd_settime(timerfd, 0, &ts, NULL) < 0) {
		perror("timerfd_settime() failed");
	}

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = timer_param;
    epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);

    while(!done) {
        while(num_connection < concurrency && num_connection < num_client) {
            if(connect_server(epfd, server_ip, server_port) < 0) {
                done = 1;
                break;
            }
        }

        //printf("\t >> Waiting for events...\n");

        nevents = epoll_wait(epfd, events, maxevent, -1);

        //printf("==============================\n");

        for (int i = 0; i < nevents; i++) {
            struct param * var = (struct param *)events[i].data.ptr;
            if ((events[i].events & EPOLLIN) && var->type == TIMER_VAR && var->sockfd == timerfd) {   
                //printf(" >> Time's up\n");
                handle_timeup_event(epfd, timerfd);
                if (num_complete == num_client) {
                    done = 1;
                    break;
                }
            }else if ((events[i].events & EPOLLERR) && var->type == SOCK_VAR) {
                //printf(" >> Close on sock %d\n", var->sockfd);
                close_connection(epfd, var->sockfd, var);
            } else if ((events[i].events & EPOLLIN) && var->type == SOCK_VAR) {
                handle_read_event(epfd, var->sockfd, var);
            } else if ((events[i].events & EPOLLOUT) && var->type == SOCK_VAR) {
                handle_write_event(epfd, var->sockfd, var);
            } else {
                printf(" >> unknown event!\n");
            }
            
        }
    }
}

int 
main(int argc, char * argv[]) {
    int i;
    char server_ip[20];
    int server_port;

    for (i = 0; i < argc; i++){
        double d;
        uint64_t n;
        char junk;
        if(sscanf(argv[i], "--num_thread=%llu%c", &n, &junk) == 1) {
            num_client = n;
            //printf("[CLIENT] thread num per core: %d\n", num_client);
        }else if(sscanf(argv[i], "--core_id=%llu%c", &n, &junk) == 1) {
            core_id = (n - 1) % CPU_NUM + 1;
            //printf("[CLIENT] core id: %d\n", core_id);
        }else if(sscanf(argv[i], "--size=%llu%c", &n, &junk) == 1) {
            buff_size = n;
            //printf("[CLIENT] buff size: %d\n", buff_size);
        }else if(sscanf(argv[i], "--time=%llu%c", &n, &junk) == 1) {
            eval_time = n;
            //printf("[CLIENT] evaluation time: %d\n", eval_time);
        }else if(sscanf(argv[i], "--server_ip=%s%c", server_ip, &junk) == 1) {
            //printf("[CLIENT] server ip: %s\n", server_ip);
        }else if(sscanf(argv[i], "--server_port=%d%c", &server_port, &junk) == 1) {
            //printf("[CLIENT] server port: %d\n", server_port);
        }else if(sscanf(argv[i], "--server_core=%d%c", &n, &junk) == 1) {
            num_server_core = n;
            //printf("[CLIENT] evaluation time: %d\n", eval_time);
        }else if(i > 0) {
            printf("error (%s)!\n", argv[i]);
        }
    }

#if TEST_FILE
    FILE * fp = fopen("input.dat", "rb");

    fread(input_file, 1, M_512, fp);

    file_ptr = input_file;

    fclose(fp);
#endif

#ifdef EVAL_RTT
    char name[20];
    sprintf(name, "rtt_core_%d.txt", core_id);
    rtt_file = fopen(name, "wb");
    fseek(rtt_file, 0, SEEK_END);

    rtt_buff = (long long *)calloc(M_128, sizeof(long long));
    rtt_buff_len = 0;
#endif

    signal(SIGINT, handle_signal);

    client_thread_arg.core_id = core_id;
    client_thread_arg.ip_addr = server_ip;
    client_thread_arg.port = server_port;
    pthread_create(&client_thread, NULL, RunClientThread, (void *)&client_thread_arg);

    pthread_join(client_thread, NULL);

}
