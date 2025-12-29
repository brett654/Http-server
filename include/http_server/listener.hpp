#ifndef LISTENER_H
#define LISTENER_H

#include <string_view>
#include <memory>
#include <string>
#include <system_error>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define MAX_QUEUE 128
#define MAX_CLIENTS 1024

static constexpr std::string_view HTTP_503_FULL = 
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Server is at capacity";

class Client;

class Listener {
private:
    int listener_fd;
    int epoll_fd;
public:
    Listener(std::string port, int epoll_fd);
    ~Listener();
    std::unique_ptr<Client> handle_new_connection();
};

#endif