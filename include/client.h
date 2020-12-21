#include "common.h"

static struct thread_context *g_ctx[MAX_CPUS] = {0};
static struct wget_stat *g_stat[MAX_CPUS] = {0};

#define MAX_URL_LEN 128
#define FILE_LEN    128
#define FILE_IDX     10
#define MAX_FILE_LEN (FILE_LEN + FILE_IDX)
#define HTTP_HEADER_LEN 1024

#define IP_RANGE 1
#define MAX_IP_STR_LEN 16

#define BUF_SIZE (8*1024)

#define CALC_MD5SUM FALSE

#define TIMEVAL_TO_MSEC(t)		((t.tv_sec * 1000) + (t.tv_usec / 1000))
#define TIMEVAL_TO_USEC(t)		((t.tv_sec * 1000000) + (t.tv_usec))
#define TS_GT(a,b)				((int64_t)((a)-(b)) > 0)

static pthread_t app_thread[MAX_CPUS];
static mctx_t g_mctx[MAX_CPUS];
static int done[MAX_CPUS];
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
/*----------------------------------------------------------------------------*/
static int fio = FALSE;
static char outfile[FILE_LEN + 1];
/*----------------------------------------------------------------------------*/
static char host[MAX_IP_STR_LEN + 1] = {'\0'};
static char url[MAX_URL_LEN + 1] = {'\0'};
static in_addr_t daddr;
static in_port_t dport;
static in_addr_t saddr;
/*----------------------------------------------------------------------------*/
static int total_flows;
static int flows[MAX_CPUS];
static int flowcnt = 0;
static int concurrency;
static int max_fds = 30000;
static uint64_t response_size = 0;

//#define RECEIVE_DEBUG

//#define __EV_RTT__

//pthread_mutex_t work_done_lock;
int work_done_flag = 0;

//pthread_mutex_t fin_client_thread_lock;
int fin_client_thread = 0;

#ifdef __EV_RTT__
pthread_mutex_t rtt_lock;
#endif

struct debug_response_arg {
    struct event * read_ev;
    struct send_info * info;
    FILE * fp;
};

struct response_arg {
    struct event * read_ev;
    struct send_info * info;
};

#define RESPONSE_ARG_SIZE sizeof(struct response_arg)

struct send_info {
    int * sockfd;
//    pthread_mutex_t * send_lock;
    int * send_byte;
//    pthread_mutex_t * recv_lock;
    int * recv_byte;
    struct hikv_arg * hikv_thread_arg;
    int thread_id;
};

struct client_vars
{
	int request_sent;

	char response[HTTP_HEADER_LEN];
	int resp_len;
	int headerset;
	uint32_t header_len;
	uint64_t file_len;
	uint64_t recv;
	uint64_t write;

	struct timeval t_start;
	struct timeval t_end;
	
	int fd;
	char * file_ptr;
};

struct client_stat
{
	uint64_t waits;
	uint64_t events;
	uint64_t connects;
	uint64_t reads;
	uint64_t writes;
	uint64_t completes;

	uint64_t errors;
	uint64_t timedout;

	uint64_t sum_resp_time;
	uint64_t max_resp_time;
};

struct thread_context
{
	int core;

	mctx_t mctx;
	int ep;
	struct client_vars *vars;

	int target;
	int started;
	int errors;
	int incompletes;
	int done;
	int pending;

	struct client_stat stat;
};

char input_file[M_512];

typedef struct thread_context* thread_context_t;

#define SEND_INFO_SIZE sizeof(struct send_info)

int connect_server(char * server_ip, int port);

void send_request_thread(struct send_info * info);

void * send_request(void * arg);

void receive_response_thread(struct send_info * info);

void response_process(int sock, short event, void * arg);

uint8_t * value_corpus;

void gen_key_corpus(LL * key_corpus, int num_put, int thread_id);
void gen_value_corpus(uint8_t * value_corpus, int num_put);

int bufcmp(char * a, char * b, int buf_len);
