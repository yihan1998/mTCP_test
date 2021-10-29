//
//  client.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/10/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_CLIENT_H_
#define YCSB_C_CLIENT_H_

#include <string>
#include "db.h"
#include "core_workload.h"


#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>

//mTCP library
#include <mtcp_api.h>
#include <mtcp_epoll.h>

#define BUFF_SIZE   1024

struct conn_info {
    int     sockfd;
    int     epfd;

    long long   total_record_ops;
    long long   total_operation_ops;

    long long   actual_record_ops;
    long long   actual_operation_ops;
};

struct thread_context {
	mctx_t              mctx;
	int                 epfd;
    int                 num_conn;
	struct conn_info    * info;
};

struct client_arg {
    int                 core;
    utils::Properties   * props;
};

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif

static pthread_t cl_thread[MAX_CPUS];
static struct client_arg cl_thread_arg[MAX_CPUS];

namespace ycsbc {
class KVRequest {
    public:
        // KVRequest(Operation op, std::string table, DB::KVPair request) : op(op), table(table), request(request) { }
        KVRequest() { }

        Operation op;
        char table[16];
        std::pair<char[100], char[100]> request;
};

class KVReply {
    public:
        // KVReply(Operation op, int return_val, DB::KVPair result) : op(op), return_val(return_val), result(result) { }
        KVReply() { }

        Operation op;
        int return_val;
        std::pair<char[100], char[100]> result;
};

class Client {
    public:
        Client(DB &db, CoreWorkload &wl) : db_(db), workload_(wl) { }
        
        virtual bool DoInsert();
        virtual bool DoTransaction();

        virtual int InsertRecord(KVRequest &request);

        virtual int SendRequest(KVRequest &request);
        virtual int ReceiveReply(KVReply &reply);

        virtual int ConnectServer(char * ip, int port);
        virtual int HandleReadEvent(struct conn_info * info);
        virtual int HandleWriteEvent(struct conn_info * info);
        virtual int HandleErrorEvent(struct conn_info * info);
        
        virtual ~Client() { }
    
    protected:
    
        virtual int ReadRequest(KVRequest &request);
        virtual int ReadModifyWriteRequest(KVRequest &request);
        virtual int ScanRequest(KVRequest &request);
        virtual int UpdateRequest(KVRequest &request);
        virtual int InsertRequest(KVRequest &request);

        virtual int ReadReply(KVReply &reply);
        virtual int ReadModifyWriteReply(KVReply &reply);
        virtual int ScanReply(KVReply &reply);
        virtual int UpdateReply(KVReply &reply);
        virtual int InsertReply(KVReply &reply);

        virtual int TransactionRead();
        virtual int TransactionReadModifyWrite();
        virtual int TransactionScan();
        virtual int TransactionUpdate();
        virtual int TransactionInsert();

        DB &db_;
        CoreWorkload &workload_;
};

inline bool Client::DoInsert() {
    std::string key = workload_.NextSequenceKey();
    std::string value;
    workload_.BuildValues(value);
    // return (db_.Insert(workload_.NextTable(), key, value) == DB::kOK);
    db_.Insert(workload_.NextTable(), key, value);

    std::cout <<  " Insert table: " <<  workload_.NextTable() << ", key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
}

inline bool Client::DoTransaction() {
  int status = -1;
  switch (workload_.NextOperation()) {
    case READ:
      status = TransactionRead();
      break;
    case UPDATE:
      status = TransactionUpdate();
      break;
    case INSERT:
      status = TransactionInsert();
      break;
    case SCAN:
      status = TransactionScan();
      break;
    case READMODIFYWRITE:
      status = TransactionReadModifyWrite();
      break;
    default:
      throw utils::Exception("Operation request is not recognized!");
  }
  assert(status >= 0);
  return (status == DB::kOK);
}

inline int Client::TransactionRead() {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();
    std::string result;
    db_.Read(table, key, result);
    std::cout <<  " Read table: " <<  workload_.NextTable() << ", key: " << key.c_str() << ", value: " << result.c_str() << "\n" << std::endl;
    return DB::kOK;
}

inline int Client::TransactionReadModifyWrite() {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();
    std::string result;

    db_.Read(table, key, result);

    std::string value;
    workload_.BuildUpdate(value);
    return db_.Update(table, key, value);
}

inline int Client::TransactionScan() {
//   const std::string &table = workload_.NextTable();
//   const std::string &key = workload_.NextTransactionKey();
//   int len = workload_.NextScanLength();
//   std::vector<std::vector<DB::KVPair>> result;
//   if (!workload_.read_all_fields()) {
//     std::vector<std::string> fields;
//     fields.push_back("field" + workload_.NextFieldName());
//     return db_.Scan(table, key, len, &fields, result);
//   } else {
//     return db_.Scan(table, key, len, NULL, result);
//   }
}

inline int Client::TransactionUpdate() {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();
    std::string value;
    workload_.BuildUpdate(value);
    return db_.Update(table, key, value);
}

inline int Client::TransactionInsert() {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextSequenceKey();
    std::string value;
    workload_.BuildValues(value);
    return db_.Insert(table, key, value);
} 

inline int Client::InsertRecord(KVRequest &request) {
    const std::string &table = workload_.NextTable();
    std::string key = workload_.NextSequenceKey();
    std::string value;
    workload_.BuildValues(value);

    request.op = INSERT;
    // request.table = table;
    // request.request = std::make_pair(key, std::string());
    strcpy(request.table, table.c_str());
    // key.copy(request.request.first, key.size());
    strcpy(request.request.first, key.c_str());
    // value.copy(request.request.second, value.size());
    strcpy(request.request.second, value.c_str());
    // std::cout <<  " Insert record to table: " << request.table << ", key: " << request.request.first << ", value: " << request.request.second << "\n" << std::endl;
    return DB::kOK;
}

inline int Client::SendRequest(KVRequest &request) {
    int status = -1;
    switch (workload_.NextOperation()) {
        case READ:
            status = ReadRequest(request);
            break;
        case UPDATE:
            status = UpdateRequest(request);
            break;
        case INSERT:
            status = InsertRequest(request);
            break;
        case SCAN:
            status = ScanRequest(request);
            break;
        case READMODIFYWRITE:
            status = ReadModifyWriteRequest(request);
            break;
        default:
            throw utils::Exception("Operation request is not recognized!");
    }
  assert(status >= 0);
  return (status == DB::kOK);
}

inline int Client::ReceiveReply(KVReply &reply) {
    int status = -1;
    switch (reply.op) {
        case READ:
            status = ReadReply(reply);
            break;
        case UPDATE:
            status = UpdateReply(reply);
            break;
        case INSERT:
            status = InsertReply(reply);
            break;
        case SCAN:
            status = ScanReply(reply);
            break;
        case READMODIFYWRITE:
            status = ReadModifyWriteReply(reply);
            break;
        default:
            throw utils::Exception("Operation request is not recognized!");
    }
    assert(status >= 0);
    return (status == DB::kOK);
}

inline int Client::HandleReadEvent(struct conn_info * info) {
    // char buff[BUFF_SIZE];

    // int len = read(info->sockfd, buff, BUFF_SIZE);
    // printf("%s\n", buff);

    // return len;
    KVReply reply;
    int len = read(info->sockfd, &reply, sizeof(reply));

    if (len <= 0) {
        return len;
    }
    
    int ret = ReceiveReply(reply);

    return len;
}

inline int Client::HandleWriteEvent(struct conn_info * info) {
    // char buff[BUFF_SIZE];
    // sprintf(buff, "Hello from client(%d)", counter++);

    // int len = send(info->sockfd, buff, BUFF_SIZE, 0);
    // printf("Hello message sent: %s\n", buff);

    // char buff[BUFF_SIZE];
    // snprintf(buff, sizeof(request), (char *)&request);
    // std::cout <<  " Send request: " << buff << "\n" << std::endl;

    KVRequest request;
    int ret = SendRequest(request);
    
    int len = write(info->sockfd, &request, sizeof(request));

    return len;
}

inline int Client::HandleErrorEvent(struct conn_info * info) {
    return 0;
}

inline int Client::ReadRequest(KVRequest &request) {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();

    request.op = READ;
    
    std::string empty;
    strcpy(request.table, table.c_str());
    strcpy(request.request.first, key.c_str());
    strcpy(request.request.second, empty.c_str());

    // std::cout <<  " Read table: " <<  request.table << ", key: " << request.request.first << "\n" << std::endl;

    return DB::kOK;
}

inline int Client::ReadReply(KVReply &reply) {
    std::string key = std::string(reply.result.first, sizeof(reply.result.first));
    std::string value = std::string(reply.result.second, sizeof(reply.result.second));

    // std::cout <<  " Read reply key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;

    return reply.return_val;
}

inline int Client::ReadModifyWriteRequest(KVRequest &request) {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();

    std::string value;
    workload_.BuildUpdate(value);

    request.op = READMODIFYWRITE;

    strcpy(request.table, table.c_str());
    strcpy(request.request.first, key.c_str());
    strcpy(request.request.second, value.c_str());
    
    // std::cout <<  " ReadModifyWrite table: " << request.table << ", key: " << request.request.first << ", value: " << request.request.second << "\n" << std::endl;
    
    return DB::kOK;
}

inline int Client::ReadModifyWriteReply(KVReply &reply) {
    std::string key = std::string(reply.result.first, sizeof(reply.result.first));
    std::string value = std::string(reply.result.second, sizeof(reply.result.second));

    // std::cout <<  " ReadModifyWrite reply key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;

    return reply.return_val;
}

inline int Client::ScanRequest(KVRequest &request) {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();
    int len = workload_.NextScanLength();
    std::string record_count = std::to_string(len);

    request.op = SCAN;

    strcpy(request.table, table.c_str());
    strcpy(request.request.first, key.c_str());
    strcpy(request.request.second, record_count.c_str());

    // std::cout <<  " Scan table: " <<  table.c_str() << ", key: " << key.c_str() << ", record len: " << len << "\n" << std::endl;
    
    return DB::kOK;
}

inline int Client::ScanReply(KVReply &reply) {
    // std::string key = result.first;
    // std::string value = result.second;
    // std::cout <<  " Scan key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
    // return return_val;
    // std::cout <<  " Scan reply " << std::endl;
    return reply.return_val;
}

inline int Client::UpdateRequest(KVRequest &request) {
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();

    std::string value;
    workload_.BuildUpdate(value);
    
    request.op = UPDATE;

    strcpy(request.table, table.c_str());
    strcpy(request.request.first, key.c_str());
    strcpy(request.request.second, value.c_str());
    
    // std::cout <<  " Update table: " <<  table.c_str() << ", key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
    
    return DB::kOK;
}

inline int Client::UpdateReply(KVReply &reply) {
    std::string key = std::string(reply.result.first, sizeof(reply.result.first));
    std::string value = std::string(reply.result.second, sizeof(reply.result.second));

    // std::cout <<  " Update reply key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
    return reply.return_val;
}

inline int Client::InsertRequest(KVRequest &request) {
    // const std::string &table = workload_.NextTable();
    // const std::string &key = workload_.NextSequenceKey();
    // std::vector<DB::KVPair> values;
    // workload_.BuildValues(values);
    // return db_.Insert(table, key, values);
    const std::string &table = workload_.NextTable();
    const std::string &key = workload_.NextTransactionKey();

    std::string value;
    workload_.BuildValues(value);
    
    request.op = INSERT;

    strcpy(request.table, table.c_str());
    strcpy(request.request.first, key.c_str());
    strcpy(request.request.second, value.c_str());
    
    // std::cout <<  " Insert table: " <<  table.c_str() << ", key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
    
    return DB::kOK;
}

inline int Client::InsertReply(KVReply &reply) {
    std::string key = std::string(reply.result.first, sizeof(reply.result.first));
    std::string value = std::string(reply.result.second, sizeof(reply.result.second));

    // std::cout <<  " Insert reply key: " << key.c_str() << ", value: " << value.c_str() << "\n" << std::endl;
    return reply.return_val;
}

inline int Client::ConnectServer(char * ip, int port) {
    int sock = 0;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
   
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
   
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    return sock;
}

} // ycsbc

#endif // YCSB_C_CLIENT_H_
