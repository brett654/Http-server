#include "../include/http_server/listener.hpp"
#include "../include/http_server/client.hpp"

Listener::Listener(std::string port, int epoll_fd)
    : epoll_fd(epoll_fd) {
    int yes = 1;

    listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd == -1) {
        throw std::system_error(errno, std::system_category(), "socket");
    }

    try {
        if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            throw std::system_error(errno, std::system_category(), "setsockopt");
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<uint16_t>(std::stoi(port)));
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listener_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            throw std::system_error(errno, std::system_category(), "bind");
        }

        if (listen(listener_fd, MAX_QUEUE) == -1) {
            throw std::system_error(errno, std::system_category(), "listen");
        }

        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.ptr = this;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener_fd, &ev) == -1) {
            throw std::system_error(errno, std::system_category(), "epoll_ctl");
        }
    } catch(...) {
        close(listener_fd);
        throw;
    }
}

Listener::~Listener() {
    if (listener_fd != -1) {
        close(listener_fd);
    }
}

std::unique_ptr<Client> Listener::handle_new_connection() {
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    int opt = 1;
    
    int client_fd = accept4(listener_fd, (struct sockaddr*)&remote_addr, &addr_len, SOCK_NONBLOCK);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return nullptr;
        }

        // Resource Exhaustion
        if (errno == EMFILE || errno == ENFILE) {
            std::cerr << "Recourse limit reached Cannot accept more clients." << std::endl;
            return nullptr; 
        }

        // Connection was aborted by client before accepted
        if (errno == ECONNABORTED || errno == EINTR) {
            return nullptr;
        }
        
        throw std::system_error(errno, std::system_category(), "accept");
    }
    /*
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        throw std::system_error(errno, std::system_category(), "setsockopt");
        return nullptr;
    }
    */

    if (client_fd >= MAX_CLIENTS) {
        send(client_fd, HTTP_503_FULL.data(), HTTP_503_FULL.length(), 0);
        close(client_fd);
        return nullptr;
    }

    return std::make_unique<Client>(client_fd, epoll_fd);
}