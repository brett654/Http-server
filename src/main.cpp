#include "../include/http_server/client.hpp"
#include "../include/http_server/listener.hpp"

#include <signal.h>
#include <sys/epoll.h>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <print>

#define PORT "9034"
#define MAX_EVENTS 100

volatile int keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

int main() {
    signal(SIGINT, handle_sigint);
    
    struct epoll_event events[MAX_EVENTS];
    std::unordered_map<int, std::unique_ptr<Client>> clients;

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return EXIT_FAILURE;
    }

    Listener listener(PORT, epoll_fd);

    while (keep_running) {
        int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_fds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_fds; i++) {
            if (events[i].data.ptr == &listener) {
                std::unique_ptr<Client> new_client = listener.handle_new_connection();
                if (new_client) {
                    int new_fd = new_client->client_fd;
                    clients[new_fd] = std::move(new_client);
                }
                continue;
            }

            Client* c = static_cast<Client*>(events[i].data.ptr);
            int client_fd = c->client_fd;

            c->process_events(events[i].events);

            if (c->state == ClientState::CLOSE) {
                clients.erase(client_fd);
                c = nullptr;
                std::printf("Connection closed on socket %d\n", client_fd);
            }
        }
    }
    clients.clear();
    return 0;
}