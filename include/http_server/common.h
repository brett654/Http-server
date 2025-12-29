#ifndef COMMON_H
#define COMMON_H

#define TIMEOUT 10
#define MAX_CLIENTS 1024
#define MAXLiNE 4096
#define DEBUG

typedef enum {
    STATE_READ_REQUEST,   // Currently receiving bytes
    STATE_PROCESS,        // Bytes received, generating response
    STATE_WRITE_RESPONSE, // Sending bytes back to client
    STATE_CLOSE           // Marking for cleanup
} ClientState;

typedef struct {
    int fd;
//    struct sockaddr_in address;
//    time_t last_activity;
    char buf[MAXLiNE];
    int bytes_read;
    int bytes_to_send;
    int bytes_sent;
    ClientState state;

    void* protocol_res;
//    void (*free_protocol_data)(void*); // function pointer to cleanup
} Client;

static const char* HTTP_503_FULL = 
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Server is at capacity";

static const char* HTTP_500_ERR = 
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

#endif