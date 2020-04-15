#include "common.h"

//mTCP library
#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include "cpu.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"

#include "mtcp.h"
#include "mtcp_api.h"
#include "tcp_in.h"
#include "tcp_stream.h"
#include "tcp_out.h"
#include "ip_out.h"
#include "eventpoll.h"
#include "pipe.h"
#include "fhash.h"
#include "addr_pool.h"
#include "rss.h"
#include "debug.h"

//HiKV library
#include "city.h"
#include "../Hikv/obj/config.h"
#include "pflush.h"
#include "random.h"
#include "pm_alloc.h"
#include "btree.h"
#include "hikv.h"

//#define __EVAL_READ__

#ifdef __EVAL_READ__
pthread_mutex_t read_cb_lock;
int read_cnt;
int read_time;
int write_cnt;
int write_time;
#endif

//#define __EVAL_CB__

#ifdef __EVAL_CB__
pthread_mutex_t read_lock;
int get_cnt;
int get_time;
#endif

//#define __REAL_TIME__

#ifdef __REAL_TIME__
pthread_mutex_t record_lock;
int request_cnt;
int byte_sent;

pthread_mutex_t start_lock;
struct timeval g_start;
int start_flag;

pthread_mutex_t end_lock;
struct timeval g_end;
#endif

#define ZERO_COPY

//#define __EVAL_KV__

#ifdef __EVAL_KV__
pthread_mutex_t record_lock;
int put_cnt;
int get_cnt;

pthread_mutex_t start_lock;
struct timeval g_start;
int start_flag;

pthread_mutex_t put_end_lock;
struct timeval put_end;
int put_end_flag;

pthread_mutex_t end_lock;
struct timeval g_end;
#endif

//#define __EVAL_FRAM__

#define BUF_SIZE 1024

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

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

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

//    struct ring_buf * recv_buf;
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

int ZeroCopyProcess(mctx_t mctx, int sockid);
struct tcp_send_buffer * GetSendBuffer(mtcp_manager_t mtcp, tcp_stream *cur_stream, int to_put);
int WriteProcess(mtcp_manager_t mtcp, struct tcp_send_buffer * buf, size_t len);
int SendProcess(mtcp_manager_t mtcp, socket_map_t socket);
