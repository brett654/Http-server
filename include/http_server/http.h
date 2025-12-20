#ifndef HTTP_H
#define HTTP_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define METHOD_LEN 9
#define PATH_LEN 257
#define VERSION_LEN 17
#define MIME_TYPE_LEN 64
#define STATUS_MESSAGE_LEN 32
#define MAX_FILE_LEN 4096

typedef enum {
    HTTP_OK = 0,
    HTTP_PARSE_ERR,
    HTTP_METHOD_NOT_SUPPORTED,
    HTTP_FILE_NOT_FOUND,
    HTTP_MALLOC_ERR,
    HTTP_FILE_READ_ERR,
    HTTP_HEADER_CREATION_ERR,
} HttpResult;

typedef struct {
    char method[METHOD_LEN];
    char path[PATH_LEN];
    char version[VERSION_LEN];
} HttpRequest;

typedef struct {
    int status_code;
    char status_message[STATUS_MESSAGE_LEN];
    char mime_type[MIME_TYPE_LEN];
    size_t content_length;
    char* body;
    size_t response_size;
    char* response_buffer;
} HttpResponse;

HttpResult http_handle_request(const char* buf, HttpResponse* http_response);
HttpResult http_serialize(HttpResponse* http_response);
HttpResponse* http_init_response();
void http_free_response(HttpResponse* http_response);
const char* http_strerror(HttpResult http_result);

#endif