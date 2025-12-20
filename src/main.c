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

#define PORT "9034"
#define MAXQUEUE 10
#define MAXLiNE 4096

int main() {
    NetResult net_result;
    HttpResult http_result;

    char buf[MAXLiNE];
    int num_bytes;
    int current_fd;

    NetContext net_ctx;
    net_init(&net_ctx);

    net_result = setup_listener_socket(PORT, &net_ctx);
    if (net_result != NET_OK) {
        fprintf(stderr, "ERROR: %s (%s)\n", net_strerror(net_result), strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (1) {
        net_ctx.read_fds = net_ctx.master;
        if (select(net_ctx.fd_max + 1, &net_ctx.read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
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
                    perror("recv");
                    disconnect_client(current_fd, &net_ctx);
                    break;
                case 0: // Connection closed
                    printf("httpserver: socket %d hung up\n", current_fd);
                    disconnect_client(current_fd, &net_ctx);
                    break;
                default: // Got data!
                    HttpResponse* http_response = http_init_response();

                    buf[num_bytes] = '\0';
                    printf("\nSent from client:\n%s", buf);

                    http_result = http_handle_request(buf, http_response);
                    if (http_result != HTTP_OK) {
                        fprintf(stderr, "ERROR: %s\n", http_strerror(http_result));
                    }

                    http_result = http_serialize(http_response);
                    if (http_result != HTTP_OK) {
                        fprintf(stderr, "ERROR: %s\n", http_strerror(http_result));
                    }
                    
                    if (send(current_fd, http_response->response_buffer, http_response->response_size, 0) == -1) {
                        perror("send");
                    }
                    
                    http_free_response(http_response);
                    break;

            }
        }
    }
}