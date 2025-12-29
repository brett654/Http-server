#ifndef CLIENT_H
#define CLIENT_H

#include "http.h"

#include <unistd.h>
#include <memory>
#include <array>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <iostream>
#include <string.h>

enum class ClientState {
    READ_REQUEST,   // Currently receiving bytes
    PROCESS,        // Bytes received, generating response
    WRITE_RESPONSE, // Sending bytes back to client
    CLOSE,    
};

class Client {
private:
    int client_fd;
    int epoll_fd;
    int bytes_read;
    int bytes_to_send;
    int bytes_sent;

    ClientState state;

    std::unique_ptr<HttpResponse, decltype(&http_free_response)> response;

    std::array<char, 4096> buf;
public:
    Client(int client_fd, int epoll_fd);
    ~Client();
    void reset();

    void handle_read_state();
    void handle_process_state();
    void handle_write_state();

    void process_events(uint32_t events);

    int get_client_fd();
    ClientState get_state();
};

#endif