#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define MAXQUEUE 10

typedef struct {
//    int fd;
//    struct sockaddr_in address;
    time_t last_activity;
} Client;

typedef struct {
    fd_set master;
    fd_set read_fds; // temp fd list for select()
    int fd_max;
    int listener; // listening socket descriptor
    Client* clients[FD_SETSIZE];
} NetContext;

typedef enum {
    NET_OK = 0,
    NET_SOCKET_ERR,
    NET_BIND_ERR,
    NET_LISTEN_ERR,
    NET_ACCEPT_ERR,
    NET_SETSOCKOPT_ERR,
    NET_MAX_CLIENTS_ERR
} NetResult;

NetResult setup_listener_socket(const char* port, NetContext* net_ctx);
NetResult handle_new_connection(NetContext* net_ctx);
const char* net_strerror(NetResult status);
void disconnect_client(int client_fd, NetContext* net_ctx);
void net_init(NetContext* net_ctx);
void server_cleanup(NetContext* net_ctx);

#endif