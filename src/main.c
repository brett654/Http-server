#include "../include/http_server/network.h"
#include "../include/http_server/http.h"
#include "../include/http_server/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
//#include <unistd.h>
//#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/epoll.h>

#define PORT "9034"

volatile int keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

int main() {
    signal(SIGINT, handle_sigint);

    NetResult net_result;
    HttpResult http_result;

    int num_bytes;
    int num_fds;
    int select_result;

    NetContext net_ctx;
    net_init(&net_ctx);

    net_ctx.epoll_fd = epoll_create1(0);
    if (net_ctx.epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    net_result = setup_listener_socket(PORT, &net_ctx);
    if (net_result != NET_OK) {
        fprintf(stderr, "ERROR: %s (%s)\n", net_strerror(net_result), strerror(errno));
        exit(EXIT_FAILURE);
    }


    while (1) {
        num_fds = epoll_wait(net_ctx.epoll_fd, net_ctx.events, MAX_EVENTS, -1);
        if (num_fds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_fds; i++) {
            if (net_ctx.events[i].data.fd == net_ctx.listener) {              
                net_result = handle_new_connection(&net_ctx);
                if (net_result != NET_OK) {
                    fprintf(stderr, "ERROR: %s (%s)\n", net_strerror(net_result), strerror(errno));
                }
                continue;
            }

            Client* c = (Client*)net_ctx.events[i].data.ptr;
            int current_fd = c->fd;

            num_bytes = recv(current_fd, c->buf, sizeof(c->buf) - 1, 0);

            switch(num_bytes) {
                case -1: // Error
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("httpserver: Request took too long\n");
                    } else {
                        perror("recv error");
                    }
                    disconnect_client(c);
                    break;
                case 0: // Connection closed
                    disconnect_client(c);
                    break;
                default: {// Got data!
                    HttpResponse* http_response = http_init_response();
                    c->buf[num_bytes] = '\0';

                    http_result = http_handle_request(c->buf, http_response);
                    if (http_result != HTTP_OK) {
                        fprintf(stderr, "ERROR: %s\n", http_strerror(http_result));
                    }

                    if (http_serialize(http_response) != HTTP_OK) {
                        send(current_fd, HTTP_500_ERR, strlen(HTTP_500_ERR), 0);
                        http_response->keep_alive = false;
                    } else {
                        if (send(current_fd, http_response->response_buffer, http_response->response_size, 0) == -1) {
                            perror("send");
                        }
                    }

                    if (!http_response->keep_alive) {
                        disconnect_client(c);
                    }
                    http_free_response(http_response);
                    break;
                }
            }
        }
    }
    system_cleanup(&net_ctx);
    return 0;
}