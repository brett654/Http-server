#include "../include/http_server/network.h"
#include "../include/http_server/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define PORT "9034"
#define MAXQUEUE 10
#define MAXLiNE 4096
#define TIMEOUT 5

int main() {
    NetResult net_result;
    HttpResult http_result;

    char buf[MAXLiNE];
    int num_bytes;
    int current_fd;
    int select_result;

    NetContext net_ctx;
    net_init(&net_ctx);

    net_result = setup_listener_socket(PORT, &net_ctx);
    if (net_result != NET_OK) {
        fprintf(stderr, "ERROR: %s (%s)\n", net_strerror(net_result), strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (1) {
        net_ctx.read_fds = net_ctx.master;

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        select_result = select(net_ctx.fd_max + 1, &net_ctx.read_fds, NULL, NULL, &timeout);
        if (select_result == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        } else if (select_result == 0) {
            printf("httpserver: idle timeout(5s) - checking for expired connections\n");

            time_t now = time(NULL);

            for (int i = 0; i < net_ctx.fd_max; i++) {
                if (i == net_ctx.listener || net_ctx.clients[i] == NULL) {
                    continue;
                }

                if (now - net_ctx.clients[i]->last_activity >= 5) {
                    printf("httpserver: Connection expired for socket: %d\n", i);
                    disconnect_client(i, &net_ctx);
                }
            }

            continue;
        }

        for (current_fd = 0; current_fd <= net_ctx.fd_max; current_fd++) {
            if (!FD_ISSET(current_fd, &net_ctx.read_fds)) {
                continue;
            }

            if (current_fd == net_ctx.listener) { // handle client sending request to join
                net_result = handle_new_connection(&net_ctx);
                if (net_result != NET_OK) {
                    fprintf(stderr, "ERROR: %s (%s)\n", net_strerror(net_result), strerror(errno));
                }
                continue;
            }
            // handle data sent from client
            num_bytes = recv(current_fd, buf, sizeof(buf) - 1, 0);

            switch(num_bytes) {
                case -1: // Error
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("LOG: Request took too long (Client timed out after 5s)\n");
                    } else {
                        perror("recv error");
                    }
                    disconnect_client(current_fd, &net_ctx);
                    break;
                case 0: // Connection closed
                    printf("httpserver: socket %d hung up\n", current_fd);
                    disconnect_client(current_fd, &net_ctx);
                    break;
                default: // Got data!
                    HttpResponse* http_response = http_init_response();
                    Client* c = net_ctx.clients[current_fd];

                    if (c != NULL) {
                        c->last_activity = time(NULL);
                    }

                    buf[num_bytes] = '\0';

                    http_result = http_handle_request(buf, http_response);
                    if (http_result != HTTP_OK) {
                        fprintf(stderr, "ERROR: %s\n", http_strerror(http_result));
                    }

                    if (http_serialize(http_response) == HTTP_OK) {
                        if (send(current_fd, http_response->response_buffer, http_response->response_size, 0) == -1) {
                            perror("send");
                        }
                    } else {
                        const char* fatal = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                        send(current_fd, fatal, strlen(fatal), 0);
                    }

                    //disconnect_client(current_fd, &net_ctx);
                    http_free_response(http_response);
                    break;

            }
        }
    }
}