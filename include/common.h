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

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include "cpu.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"

#include "size.h"

#define LL long long

#define NUM_ITER 1000000000

#define PUT_PERCENT 50			// Percentage of PUT operations

#define NUM_KEYS M_1			// 51 * M_4 ~ 200 M keys
#define NUM_KEYS_ M_1_

#define SWAP(a,b) do{a^=b;b^=a;a^=b;}while(0)

#define VALUE_SIZE 256
#define KEY_SIZE 64

// The key-value struct in network connection
struct __attribute__((__packed__)) kv_trans_item {
	uint16_t len;
	uint8_t value[VALUE_SIZE];
	uint8_t key[KEY_SIZE];
};

#define KV_ITEM_SIZE sizeof(struct kv_trans_item)

struct client_arg {
    char ** ip_addr;
    int port;
    int buf_size;
};
