#ifndef COMMON_H
#define COMMON_H

#define TIMEOUT 10
#define MAX_CLIENTS 1024
#define MAXLiNE 4096
#define DEBUG

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