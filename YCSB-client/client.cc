//
//  ycsbc.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/19/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <future>
#include "core/utils.h"
#include "core/timer.h"
#include "core/client.h"
#include "core/core_workload.h"
#include "db/db_factory.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>

#define MAX_CONNECT 4100
#define MAX_EVENTS  8192

static char * conf_file = NULL;

__thread int num_conn = 0;

using namespace std;

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
string ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

double LoadRecord(int epfd, struct epoll_event * events, ycsbc::Client &client, const int num_record_ops, const int num_operation_ops, const int port, const int num_flows) {
    int record_per_flow = num_record_ops / num_flows;
    int operation_per_flow = num_operation_ops / num_flows;

    int done = 0;

    int nevents;

    int num_load_complete = 0;

    utils::Timer<double> timer;
    double duration;

    timer.Start();

    while(!done) {
        while(num_conn < num_flows) {
            /* Connect server */
            int sock;
            if ((sock = client.ConnectServer("10.0.0.1", port)) > 0) {
                // fprintf(stdout, " [%s] connect server through sock %d\n", __func__, sock);
                struct conn_info * conn_info = &info[num_conn];
                conn_info->sockfd = sock;
                conn_info->epfd = epfd;

                conn_info->total_record_ops = record_per_flow;
                conn_info->total_operation_ops = operation_per_flow;

                conn_info->actual_record_ops = conn_info->actual_operation_ops = 0;

                num_conn++;

                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLOUT;
                ev.data.ptr = conn_info;
                epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);
                mtcp_epoll_ctl(ctx->mctx, ctx->epfd, MTCP_EPOLL_CTL_ADD, c, &ev);
            } else {
                fprintf(stderr, " [%s] connect server failed!", __func__);
                exit(1);
            }
        }

        nevents = mtcp_epoll_wait(ctx->mctx, ctx->epfd, events, MAX_EVENTS, -1);

        for (int i = 0; i < nevents; i++) {
            struct conn_info * info = (struct conn_info *)(events[i].data.ptr);
            int ret;
            if ((events[i].events & EPOLLERR)) {
                client.HandleErrorEvent(info);
            }
            
            if ((events[i].events & EPOLLIN)) {
                ycsbc::KVReply reply;
                int len = read(info->sockfd, &reply, sizeof(reply));

                if (len > 0) {
                    client.ReceiveReply(reply);

                    /* Increase actual ops */
                    if(++info->actual_record_ops == info->total_record_ops) {
                        // cerr << " [ sock " << info->sockfd << "] # Loading records " << info->sockfd << " \t" << info->actual_record_ops << flush;
                        // fprintf(stdout, " [sock %d] # Loading records :\t %lld\n", info->sockfd, info->actual_record_ops);  
                        if (++num_load_complete == num_conn) {
                            done = 1;
                        }
                    }
                    
                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLOUT;
                    ev.data.ptr = info;

                    epoll_ctl(info->epfd, EPOLL_CTL_MOD, info->sockfd, &ev);
                }
            } else if ((events[i].events & EPOLLOUT)) {
                ycsbc::KVRequest request;
                client.InsertRecord(request);

                int len = send(info->sockfd, &request, sizeof(request), 0);
            
                if(len > 0) {
                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.ptr = info;

                    epoll_ctl(info->epfd, EPOLL_CTL_MOD, info->sockfd, &ev);
                }
            } else {
                printf(" >> unknown event!\n");
            }
        }
    }

    duration = timer.End();

    for (int i = 0; i < num_conn; i++) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.ptr = &info[i];

        epoll_ctl(info[i].epfd, EPOLL_CTL_MOD, info[i].sockfd, &ev);
    }
    
    return duration;
}

double PerformTransaction(int epfd, struct epoll_event * events, ycsbc::Client &client) {
    int done = 0;

    utils::Timer<double> timer;
    double duration;

    int num_transaction_complete = 0;

    int nevents;

    int oks = 0;

    timer.Start();

    while(!done) {
        nevents = epoll_wait(epfd, events, MAX_EVENTS, -1);

        for (int i = 0; i < nevents; i++) {
            struct conn_info * info = (struct conn_info *)(events[i].data.ptr);
            int ret;
            if ((events[i].events & EPOLLERR)) {
                client.HandleErrorEvent(info);
            }
            
            if ((events[i].events & EPOLLIN)) {
                ret = client.HandleReadEvent(info);
                if (ret > 0) {
                    /* Increase actual ops */
                    oks++;
                    if(++info->actual_operation_ops == info->total_operation_ops) {
                        shutdown(info->sockfd, SHUT_WR);
                        if (++num_transaction_complete == num_conn) {
                            done = 1;
                        }
                        close(info->sockfd);
                    }

                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLOUT;
                    ev.data.ptr = info;

                    epoll_ctl(info->epfd, EPOLL_CTL_MOD, info->sockfd, &ev);
                }
            } else if ((events[i].events & EPOLLOUT)) {
                ret = client.HandleWriteEvent(info);
                if(ret > 0) {
                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.ptr = info;

                    epoll_ctl(info->epfd, EPOLL_CTL_MOD, info->sockfd, &ev);
                }
            } else {
                printf(" >> unknown event!\n");
            }

        }
    }

    fprintf(stdout, " # Transaction: %llu\n", oks);

    duration = timer.End();
    return duration;
}

struct thread_context * InitializeClientThread(int core){
	struct thread_context * ctx;

	/* affinitize application thread to a CPU core */
	mtcp_core_affinitize(core);

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
	ctx->epfd = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
	if (ctx->epfd < 0) {
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create epoll descriptor!\n");
		return NULL;
	}

	/* allocate memory for server variables */
	ctx->cvars = (struct server_vars *)calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
	if (!ctx->cvars) {
		mtcp_close(ctx->mctx, ctx->epfd);
		mtcp_destroy_context(ctx->mctx);
		free(ctx);
		TRACE_ERROR("Failed to create server_vars struct!\n");
		return NULL;
	}

	return ctx;
}

int RunClientThread(void * arg) {
    struct server_arg * sarg = (struct server_arg *)arg;

    int core = sarg->core;
    utils::Properties * props = sarg->props;

	struct mtcp_epoll_event * events;
	int nevents;
	int i, ret;

    /* mTCP initialization */
    struct thread_context * ctx;
	ctx = InitializeClientThread(core);
	if (!ctx) {
		TRACE_ERROR("Failed to initialize server thread.\n");
		return NULL;
	}

    /* Initialize connection info array */
    ctx->info = (struct conn_info *)calloc(MAX_CONNECT, sizeof(struct conn_info));

    int epfd;
    struct mtcp_epoll_event * events;

    epfd = ctx->epfd;
    /* Initialize epoll event array */
    events = (struct mtcp_epoll_event *)calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));

    ycsbc::CoreWorkload wl;
    wl.Init(props);

    const int num_flows = stoi(props.GetProperty("flows", "1"));

    int record_total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
    int operation_total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
    fprintf(stdout, " [core %d] # Total records (K) :\t %.2f \n", core_id, (double)record_total_ops / 1000.0);  
    fprintf(stdout, " [core %d] # Total transactions (K) :\t %.2f\n", core_id, (double)operation_total_ops / 1000.0);  

    // double duration = DelegateClient(db, &wl, record_total_ops, operation_total_ops, num_flows);

    ycsbc::Client client(*db, wl);
    
    int port = stoi(props.GetProperty("port", "80"));

    double load_duration = 0.0;
    load_duration = LoadRecord(epfd, events, client, record_total_ops, operation_total_ops, port, num_flows);

    fprintf(stdout, " [core %d] loaded records done! \n", core_id);  

    double transaction_duration = 0.0;
    transaction_duration = PerformTransaction(epfd, events, client);

    char output[256];

    char output_file_name[32];
	sprintf(output_file_name, "throughput_core_%d.txt", core_id);
	FILE * output_file = fopen(output_file_name, "a+");

    sprintf(output, " [core %d] # Transaction throughput : %.2f (KTPS) \t %s \t %s \t %d\n", \
                    core_id, operation_total_ops / transaction_duration / 1000, props["dbname"].c_str(), \
                    file_name.c_str(), num_flows);

    fprintf(stdout, "%s", output);
    fflush(stdout);

    fprintf(output_file, "%s", output);
	fclose(output_file);

    db->Close();
}

int main(const int argc, const char *argv[]) {
    int ret;
	struct mtcp_conf mcfg;
    char * conf_file;
	int process_cpu = -1;

	char conf_name[] = "client.conf";
	conf_file = conf_name;

    for (int i = 0; i < argc; i++){
        long long unsigned n;
        char junk;
		char s[20];
        if(sscanf(argv[i], "--num_cores=%llu", &n) == 1){
            num_cores = n;
			printf(" >> core num: %d\n", num_cores);
			if (num_cores > MAX_CPUS) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUs: %d\n", MAX_CPUS);
				return FALSE;
			}
			mtcp_getconf(&mcfg);
			mcfg.num_cores = num_cores;
			mtcp_setconf(&mcfg);
        } else if(sscanf(argv[i], "--num_flow=%llu%c", &n, &junk) == 1) {
            num_flow = n; 
			printf(" >> flow num: %d\n", num_flow);
        } else if(sscanf(argv[i], "--size=%llu%c", &n, &junk) == 1) {
            buff_size = n;
			printf(" >> buff size: %d\n", buff_size);
        } else if(sscanf(argv[i], "--time=%llu", &n) == 1){
            execution_time = n;
			printf(" >> total time of execution: %d\n", execution_time);
        } else if(sscanf(argv[i], "--server_ip=%s%c", server_ip, &junk) == 1) {
            printf(" >> server ip: %s\n", server_ip);
        } else if(sscanf(argv[i], "--server_port=%d%c", &server_port, &junk) == 1) {
            printf(" >> server port: %d\n", server_port);
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
	mcfg.max_concurrency = max_fds;
	mcfg.max_num_buffers = max_fds;
	mtcp_setconf(&mcfg);
	
	/* register signal handler to mtcp */
	mtcp_register_signal(SIGINT, SignalHandler);

	TRACE_INFO("Application initialization finished.\n");

	for (int i = ((process_cpu == -1) ? 0 : process_cpu); i < num_cores; i++) {
		cl_thread_arg[i].core = i;
        cl_thread_arg[i].props = &props;

		if (pthread_create(&app_thread[i], NULL, RunClientThread, (void *)&cl_thread_arg[i])) {
			perror("pthread_create");
			TRACE_CONFIG("Failed to create server thread.\n");
				exit(EXIT_FAILURE);
		}
		if (process_cpu != -1)
			break;
	}
	
	for (int i = ((process_cpu == -1) ? 0 : process_cpu); i < num_cores; i++) {
		pthread_join(app_thread[i], NULL);

		if (process_cpu != -1)
			break;
	}

	printf(" [%s] Test finished!\n", __func__);
	
	mtcp_destroy();

	printf(" [%s] mTCP destroyed\n", __func__);

	return 0;
}

void UsageMessage(const char *command) {
    cout << "Usage: " << command << " [options]" << endl;
    cout << "Options:" << endl;
    cout << "  -threads n: execute using n threads (default: 1)" << endl;
    cout << "  -db dbname: specify the name of the DB to use (default: basic)" << endl;
    cout << "  -P propertyfile: load properties from the given file. Multiple files can" << endl;
    cout << "                   be specified, and will be processed in the order specified" << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
    return strncmp(str, pre, strlen(pre)) == 0;
}

