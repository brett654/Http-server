#include "../include/http_server/network.h"

void net_init(NetContext* net_ctx) {
    net_ctx->listener = -1;
    net_ctx->epoll_fd = -1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        net_ctx->clients[i] = NULL;
    }
}

NetResult setup_listener_socket(const char* port, NetContext* net_ctx) {
    struct sockaddr_in server_addr;
    int listener;
    int yes = 1;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        return NET_SOCKET_ERR;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        close(listener);
        return NET_SETSOCKOPT_ERR;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&server_addr.sin_zero, '\0', 8);

    if (bind(listener, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(listener);
        return NET_BIND_ERR;
    }

    if (listen(listener, MAXQUEUE) == -1) {
        close(listener);
        return NET_LISTEN_ERR;
    }

    net_ctx->ev.events = EPOLLIN;
    net_ctx->ev.data.fd = listener;
    if (epoll_ctl(net_ctx->epoll_fd, EPOLL_CTL_ADD, listener, &net_ctx->ev) == -1) {
        return NET_EPOLL_ERR;
    }

    net_ctx->listener = listener;
    return NET_OK;
}

int setnonblocking(int sock) {
    int result;
    int flags;

    flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        return -1;  // error
    }

    flags |= O_NONBLOCK;

    result = fcntl(sock , F_SETFL , flags);
    return result;
}

int handle_new_connection(NetContext* net_ctx) {
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    
    int conn_sock = accept(net_ctx->listener, (struct sockaddr*)&remote_addr, &addr_len);
    if (conn_sock == -1) {
        return -1;
    }

    setnonblocking(conn_sock);

    if (conn_sock >= MAX_CLIENTS) {
        char dummy_buffer[1024];
        recv(conn_sock, dummy_buffer, sizeof(dummy_buffer), MSG_DONTWAIT);
        send(conn_sock, HTTP_503_FULL, strlen(HTTP_503_FULL), 0);
        close(conn_sock);
        return NET_MAX_CLIENTS_ERR;
    }

    Client* c = malloc(sizeof(Client));
    if (!c) {
        send(conn_sock, HTTP_500_ERR, strlen(HTTP_500_ERR), 0);
        close(conn_sock);
        return NET_MALLOC_ERR;
    }
    
    c->fd = conn_sock;
    c->bytes_read = 0;
    c->bytes_sent = 0;
    c->bytes_to_send = 0;
    c->protocol_res = NULL;
    net_ctx->clients[conn_sock] = c;

    net_ctx->ev.events = EPOLLIN | EPOLLET;
    net_ctx->ev.data.ptr = c; // data is a union so no longer can use .fd
    if (epoll_ctl(net_ctx->epoll_fd, EPOLL_CTL_ADD, conn_sock, &net_ctx->ev) == -1) {
        perror("epoll_ctl: listener");
        return NET_EPOLL_ERR;
    } 

    #ifdef DEBUG
    printf("httpserver: new conncetion  on socket %d\n", conn_sock);
    #endif
}

void disconnect_client(Client* c) {
    if (!c) return;

    #ifdef DEBUG
    printf("httpserver: closed conncetion on socket %d\n", c->fd);
    #endif

    close(c->fd);

    if (c->protocol_res) {
        free(c->protocol_res);
        c->protocol_res = NULL;
    }
    free(c);
}

void system_cleanup(NetContext* net_ctx) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (net_ctx->clients[i] != NULL) {
            close(net_ctx->clients[i]->fd);
            free(net_ctx->clients[i]);
            net_ctx->clients[i] = NULL;
        }
    }
}

const char* net_strerror(NetResult status) {
    switch (status) {
        case NET_OK:              return "Success";
        case NET_SOCKET_ERR:      return "Failed to create socket";
        case NET_BIND_ERR:        return "Failed to bind to port";
        case NET_LISTEN_ERR:      return "Failed to listen on socket";
        case NET_SETSOCKOPT_ERR:  return "Failed to set socket options";
        case NET_ACCEPT_ERR:      return "Failed to accept new connection";
        case NET_MAX_CLIENTS_ERR: return "Max client(1024) reached";
        case NET_EPOLL_ERR:       return "Failed to set up epoll for listener socket";
        case NET_MALLOC_ERR:      return "Malloc failed";
        default:                  return "Unknown network error";
    }
}