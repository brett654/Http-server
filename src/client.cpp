#include "../include/http_server/client.hpp"

Client::Client(int client_fd, int epoll_fd) 
    : client_fd(client_fd), 
      epoll_fd(epoll_fd), 
      bytes_read(0), 
      bytes_sent(0), 
      bytes_to_send(0), 
      state(ClientState::READ_REQUEST),
      response(nullptr, http_free_response) {
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.ptr = this;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        throw std::system_error(errno, std::system_category(), "epoll ctl add");
    }
}

Client::~Client() {
    if (client_fd != -1) {
        close(client_fd);
    }
}

void Client::reset() {
    //std::fill(buf.begin(), buf.end(), 0);

    bytes_read = 0;
    bytes_sent = 0;
    bytes_to_send = 0;

    response.reset();
    state = ClientState::READ_REQUEST;
}

void Client::handle_read_state() {
    int num_bytes = recv(client_fd, buf.data() + bytes_read, buf.size() - bytes_read - 1, 0);
    if (num_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        std::cerr << "recv" << ": " << strerror(errno) << std::endl;
        state = ClientState::CLOSE;
        return;
    }

    if (num_bytes == 0) {
        state = ClientState::CLOSE;
        return;
    }

    bytes_read += num_bytes;

    std::string_view current_data(buf.data(), num_bytes);
    if (current_data.find("\r\n\r\n") != std::string_view::npos) {
        state = ClientState::PROCESS;
        return;
    }
}

void Client::handle_process_state() {
    HttpResult http_result;
    if (response == nullptr) {
        response.reset(http_init_response());
    }

    http_result = http_handle_request(buf.data(), response.get());
    if (http_result != HTTP_OK) {
       std::cerr << "request handler" << ": " << http_strerror(http_result) << std::endl;
    }

    http_result = http_serialize(response.get());
    if (http_result != HTTP_OK) {
        std::cerr << "serializer" << ": " << http_strerror(http_result) << std::endl;
        send(client_fd, HTTP_500_ERR, strlen(HTTP_500_ERR), 0);
        state = ClientState::CLOSE;
        return;
    }

    bytes_to_send = response->response_size;
    bytes_sent = 0;
    state = ClientState::WRITE_RESPONSE;
}

void Client::handle_write_state() {
    const char* send_ptr = response->response_buffer + bytes_sent;
    int remaining = bytes_to_send - bytes_sent;
    
    int num_bytes = send(client_fd, send_ptr, remaining, 0);
    if (num_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        state = ClientState::CLOSE;
        return;
    }

    bytes_sent += num_bytes;

    if (bytes_sent >= bytes_to_send) {
        if (response && response->keep_alive) {
            reset();
        } else {
            state = ClientState::CLOSE;
        }
    }
}

void Client::process_events(uint32_t events) {
    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        state = ClientState::CLOSE;
        return;
    }
    if (state == ClientState::READ_REQUEST) {
        handle_read_state();
        //std::cout << "request read on socket" << client_fd << std::endl;
    }

    if (state == ClientState::PROCESS) {
        handle_process_state();
        //std::cout << "request processesed on socket" << client_fd << std::endl;
    }

    if (state == ClientState::WRITE_RESPONSE) {
        handle_write_state();
        //std::cout << "request written on socket " << client_fd << std::endl;
    }
}

int Client::get_client_fd() {
    return client_fd;
}

ClientState Client::get_state() {
    return state;
}