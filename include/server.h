#include "common.h"

//HiKV library
#include "city.h"
#include "../Hikv/obj/config.h"
#include "pflush.h"
#include "random.h"
#include "pm_alloc.h"
#include "hikv.h"

//#define __REAL_TIME_STATS__

//#define __EVAL_FRAM__

#define BUF_SIZE 4096

#define MAX_FLOW_NUM  (10000)

#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define HT_SUPPORT FALSE

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

#ifndef MIN
#define MIN(v1, v2)	((v1) < (v2) ? (v1) : (v2))
#endif

struct thread_context {
	mctx_t mctx;
	int ep;
	struct server_vars *svars;
};

struct server_vars {
	int recv_len;
	int request_len;
	long int total_read, total_sent;
	uint8_t done;
	uint8_t rspheader_sent;
	uint8_t keep_alive;

	int total_time;
};

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

static int num_cores;
static int core_limit;
static pthread_t app_thread[MAX_CPUS];
static int done[MAX_CPUS];
static char *conf_file = NULL;
static int backlog = -1;

//static pthread_t sv_thread[MAX_CPUS];
static struct server_arg sv_thread_arg[MAX_CPUS];

static struct hikv * hi;
static struct hikv_arg * hikv_args;

void * server_thread(void * arg);