#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>

#include "sizes.h"

#define LL long long

#define NUM_ITER 1000000000

#define PUT_PERCENT 50			// Percentage of PUT operations

#define REPLY_SIZE 50

#define NUM_KEYS M_1			// 51 * M_4 ~ 200 M keys
#define NUM_KEYS_ M_1_

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

#define VALUE_SIZE 256
#define KEY_SIZE 64

#define BATCHED_KEY

#ifdef BATCHED_KEY
#define NUM_BATCH 1
#endif

#define __TEST_FILE__
//#define __TEST_KV__

//#define ZERO_COPY

// The key-value struct in network connection
struct __attribute__((__packed__)) kv_trans_item {
	uint8_t key[KEY_SIZE];
	uint8_t value[VALUE_SIZE];
};

#define KV_ITEM_SIZE sizeof(struct kv_trans_item)

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

//Server args
struct hikv_arg {
    size_t pm_size;
    uint64_t num_server_thread;
    uint64_t num_backend_thread;
    uint64_t num_warm_kv;
    uint64_t num_put_kv;
    uint64_t num_get_kv;
    uint64_t num_delete_kv;
    uint64_t num_scan_kv;
    uint64_t scan_range;
    uint64_t seed;
    uint64_t scan_all;
};

#define HIKV_ARG_SIZE sizeof(struct hikv_arg)

struct server_arg {
    int core;
    int thread_id;
//    struct hikv * hi;
//    struct hikv_arg hikv_args;
};

//Client args
struct client_arg {
    int thread_id;
    char * ip_addr;
    int port;
//    int buf_size;
    struct hikv_arg hikv_thread_arg;
};

static pthread_t cl_thread[110];
static struct client_arg cl_thread_arg[110];
