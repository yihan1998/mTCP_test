#include "client.h"

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

//    evutil_make_socket_nonblocking(sockfd);

    return sockfd;

}

void * send_request(void * arg){
    struct send_info * info = (struct send_info *)arg;

    int fd = *(info->sockfd);

    char send_buf[buf_size];
    char recv_buf[buf_size + 1];
    memset(recv_buf, 0, sizeof(recv_buf));

	int send_size, recv_size;

    FILE * send_fp = fopen("client-input.dat", "rb");
#ifdef RECEIVE_DEBUG
    FILE * recv_fp = fopen("server-ouput.dat", "wb");
#endif

#ifdef __EV_RTT__
    FILE * fp = fopen("rtt.txt", "a+");
    fseek(fp, 0, SEEK_END);
#endif

    struct timeval time1;
    gettimeofday(&time1, NULL);

    while(!feof(send_fp)){

        struct timeval start;
        gettimeofday(&start, NULL);
  
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

        struct timeval end;
        gettimeofday(&end, NULL);

#ifdef __EV_RTT__
        double start_time = (double)start.tv_sec * 1000000 + (double)start.tv_usec;
        double end_time = (double)end.tv_sec * 1000000 + (double)end.tv_usec;

        char buff[1024];

        sprintf(buff, "rtt %d\n", (int)(end_time - start_time));
        
        pthread_mutex_lock(&rtt_lock);

        fwrite(buff, strlen(buff), 1, fp);
        fflush(fp);

        pthread_mutex_unlock(&rtt_lock);
#endif

        if(end.tv_sec - time1.tv_sec > 10){
            printf("[CLIENT] request complete\n");
            return NULL;
        }
    }
    
    fclose(send_fp);

#ifdef __EV_RTT__
    fclose(fp);
#endif

    return NULL;
}

void * client_thread(void * argv){
    int buf_size = *((int *)argv);
    
    int send_byte, recv_byte;
    send_byte = recv_byte = 0;

#ifdef __EV_RTT__
    pthread_mutex_init(&rtt_lock, NULL);
#endif

    int sockfd = connect_server(inet_addr("192.168.3.2"), 12345);
    if(sockfd == -1){
        perror("[CLIENT] tcp connect error");
        exit(1);
    }

    struct send_info * info = (struct send_info *)malloc(SEND_INFO_SIZE);
    info->sockfd = &sockfd;
    info->send_byte = &send_byte;
    info->recv_byte = &recv_byte;

    send_request(info);

    free(info);

    return NULL;
}

int main(int argc, char * argv[]){
    client_thread_num = atoi(argv[1]);

    pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * client_thread_num);

    int i;
    for(i = 0;i < client_thread_num;i++){
        int * buf_size = (int *)malloc(sizeof(int));
        *buf_size = atoi(argv[2]);
#ifdef __BIND_CORE__
        arg.sequence = i;
#endif
        pthread_create(&threads[i], NULL, client_thread, (void *)buf_size);
    }

    for(i = 0;i < client_thread_num;i++){
        pthread_join(threads[i], NULL);
    }

    return 0;
}