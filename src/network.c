#include "../include/http_server/network.h"

void net_init(NetContext* net_ctx) {
    FD_ZERO(&net_ctx->master);
    FD_ZERO(&net_ctx->read_fds);
    net_ctx->fd_max = 0;
    net_ctx->listener = -1;

    for (int i = 0; i < FD_SETSIZE; i++) {
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

    net_ctx->listener = listener;
    FD_SET(listener, &net_ctx->master);
    net_ctx->fd_max = listener;
    return NET_OK;
}

NetResult handle_new_connection(NetContext* net_ctx) {
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    
    int new_fd = accept(net_ctx->listener, (struct sockaddr*)&remote_addr, &addr_len);
    if (new_fd == -1) {
        return NET_ACCEPT_ERR;
    }

    if (new_fd >= FD_SETSIZE) {
        close(new_fd);
        return NET_MAX_CLIENTS_ERR;
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    if (setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt timeout");
    }

    Client* c = malloc(sizeof(Client));
//    c->fd = new_fd;
//    c->address = remote_addr;
    c->last_activity = time(NULL);

    if (net_ctx->clients[new_fd] != NULL) {
        free(net_ctx->clients[new_fd]); 
    }
    net_ctx->clients[new_fd] = c;

    FD_SET(new_fd, &net_ctx->master);
    if (new_fd > net_ctx->fd_max) {
        net_ctx->fd_max = new_fd;
    }
    printf("httpserver: new conncetion from %s on socket %d\n",
        inet_ntoa(remote_addr.sin_addr), new_fd);
    return NET_OK;
}

void disconnect_client(int client_fd, NetContext* net_ctx) {
    FD_CLR(client_fd, &net_ctx->master);

    if (net_ctx->clients[client_fd] != NULL) {
        free(net_ctx->clients[client_fd]);
        net_ctx->clients[client_fd] = NULL;
    }

    if (client_fd == net_ctx->fd_max) {
        // loop until finding a set bit
        while (FD_ISSET(net_ctx->fd_max, &net_ctx->master) == 0 && net_ctx->fd_max > 0) {
            (net_ctx->fd_max)--;
        }
    }

    close(client_fd);
    printf("httpserver: closed conncetion on socket %d\n\n",
    client_fd);
}

void server_cleanup(NetContext* net_ctx) {
    printf("\nShutting down: Cleaning up memory...\n");
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (net_ctx->clients[i] != NULL) {
            disconnect_client(i, net_ctx);
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
        default:                  return "Unknown network error";
    }
}