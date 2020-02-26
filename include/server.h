#include "common.h"

#define BUF_SIZE 4096

struct server_vars {
	char request[HTTP_HEADER_LEN];
	int recv_len;
	int request_len;
	long int total_read, total_sent;
	uint8_t done;
	uint8_t rspheader_sent;
	uint8_t keep_alive;
};

void * server_thread(void * arg);