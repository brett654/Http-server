#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"

#define MAXQUEUE 128
#define MAX_EVENTS 100

typedef struct {
    int fd;
//    struct sockaddr_in address;
//    time_t last_activity;
    char buf[MAXLiNE];
} Client;

typedef struct {
    int listener; // listening socket descriptor
    int epoll_fd;
    Client* clients[MAX_CLIENTS];
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
} NetContext;

typedef enum {
    NET_OK = 0,
    NET_SOCKET_ERR,
    NET_BIND_ERR,
    NET_LISTEN_ERR,
    NET_ACCEPT_ERR,
    NET_SETSOCKOPT_ERR,
    NET_MAX_CLIENTS_ERR,
    NET_EPOLL_ERR,
    NET_MALLOC_ERR,
} NetResult;

NetResult setup_listener_socket(const char* port, NetContext* net_ctx);
NetResult handle_new_connection(NetContext* net_ctx);
const char* net_strerror(NetResult status);
void disconnect_client(Client* c);
void net_init(NetContext* net_ctx);
void system_cleanup(NetContext* net_ctx);

#endif