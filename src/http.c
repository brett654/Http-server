#include "../include/http_server/http.h"

HttpResponse* http_init_response() {
    HttpResponse* http_response = calloc(1, sizeof(HttpResponse));
    if (http_response == NULL) {
        return NULL;
    }

    http_response->body = NULL;
    http_response->content_length = 0;

    return http_response;
}

void http_free_response(HttpResponse* http_response) {
    if (http_response == NULL) return;

    if (http_response->body != NULL) {
        free(http_response->body);
        http_response->body = NULL;
    }

    if (http_response->response_buffer != NULL) {
        free(http_response->response_buffer);
        http_response->response_buffer = NULL;
    }

    free(http_response);
}

HttpResult http_parse_request(char* buf, HttpRequest* http_request) {
    char *token;
    char *string = buf;
    int written;

    // Extract Method
    token = strsep(&string, " ");
    if (!token) return HTTP_PARSE_ERR;
    written = snprintf(http_request->method, sizeof(http_request->method),
        "%s",
        token
    );
    if (written >= (int)sizeof(http_request->method)) return HTTP_URI_TOO_LONG;

    // Extract Path
    token = strsep(&string, " ");
    if (!token) return HTTP_PARSE_ERR;
    written = snprintf(http_request->path, sizeof(http_request->path),
        "%s",
        token
    );
    if (written >= (int)sizeof(http_request->path)) return HTTP_URI_TOO_LONG;

    // Extract Version
    token = strsep(&string, "\r\n");
    if (!token) return HTTP_PARSE_ERR;
    written = snprintf(http_request->version, sizeof(http_request->version),
        "%s",
        token
    );
    if (written >= (int)sizeof(http_request->version)) return HTTP_URI_TOO_LONG;


    if (strncmp(http_request->method, "GET", 3) != 0) {
        return HTTP_METHOD_NOT_SUPPORTED;
    }

    if (strncmp(http_request->version, "HTTP/1.1", 8) != 0 &&
        strncmp(http_request->version, "HTTP/1.0", 8) != 0
    ) {
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    return HTTP_OK;
}

HttpResult http_get_mime_type(const char* file_path, HttpResponse* http_response) {
    static MimeMap mime_types[] = {
        {"html", "text/html"},
        {"htm",  "text/html"},
        {"css",  "text/css"},
        {"js",   "application/javascript"},
        {"png",  "image/png"},
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif",  "image/gif"},
        {"json", "application/json"},
        {"txt",  "text/plain"},
    };

    const size_t mime_types_count = sizeof(mime_types) / sizeof(MimeMap);
    const size_t type_size = sizeof(http_response->mime_type);

    char* dot = strrchr(file_path, '.');
    if (!dot) {
        snprintf(http_response->mime_type, type_size, "%s", "application/octet-stream");
        return HTTP_OK;
    }

    char* ext = dot + 1;
    
    for (int i = 0; i < mime_types_count; i++) {
        if (strcmp(ext, mime_types[i].extension) == 0) {
            snprintf(http_response->mime_type, type_size, "%s", mime_types[i].mime_type);
            return HTTP_OK;
        }
    }

    snprintf(http_response->mime_type, type_size, "%s", "application/octet-stream");
    return HTTP_OK;
}

HttpResult http_handle_file_request(char* requested_path, HttpResponse* http_response) {
    char actual_path[PATH_LEN];
    snprintf(actual_path, sizeof(actual_path), "public%s", requested_path);;

    FILE* file = fopen(actual_path, "rb");
    if (file == NULL) {
        return HTTP_FILE_NOT_FOUND;
    }

    fseek(file, 0, SEEK_END);
    size_t fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    http_response->content_length = fsize;

    http_response->body = malloc(fsize);
    if (http_response->body == NULL) {
        fclose(file);
        return HTTP_MALLOC_ERR;
    }

    size_t bytes_read = fread(http_response->body, 1, fsize, file);
    if (bytes_read < fsize) {
        free(http_response->body);
        http_response->body = NULL;
        fclose(file);
        return HTTP_FILE_READ_ERR;
    }

    fclose(file);
    return HTTP_OK;
}

void http_status_from_result(HttpResult result, HttpResponse* http_response) {
    switch(result) {
        case HTTP_OK:
            http_response->status_code = 200;
            strcpy(http_response->status_message, "OK");
            break;

        case HTTP_PARSE_ERR:
            http_response->status_code = 400;
            strcpy(http_response->status_message, "Bad Request");
            break;

        case HTTP_FORBIDDEN:
            http_response->status_code = 403;
            strcpy(http_response->status_message, "Forbidden");
            break;

        case HTTP_FILE_NOT_FOUND:
            http_response->status_code = 404;
            strcpy(http_response->status_message, "Not Found");
            break;

        case HTTP_METHOD_NOT_SUPPORTED:
            http_response->status_code = 405;
            strcpy(http_response->status_message, "Method Not Allowed");
            break;

        case HTTP_URI_TOO_LONG:
            http_response->status_code = 414;
            strcpy(http_response->status_message, "URI Too Long");
            break;

        case HTTP_VERSION_NOT_SUPPORTED:
            http_response->status_code = 505;
            strcpy(http_response->status_message, "HTTP Version Not Supported");
            break;

        case HTTP_MALLOC_ERR:
        case HTTP_FILE_READ_ERR:
        case HTTP_HEADER_CREATION_ERR:
        default:
            http_response->status_code = 500;
            strcpy(http_response->status_message, "Internal Server Error");
            break;
    }
}

const char* http_strerror(HttpResult http_result) {
    switch (http_result) {
        case HTTP_OK:                    return "Success";
        case HTTP_PARSE_ERR:             return "Failed to parse request";
        case HTTP_METHOD_NOT_SUPPORTED:  return "Http method not supported";
        case HTTP_FILE_NOT_FOUND:        return "File not found";
        case HTTP_MALLOC_ERR:            return "Malloc error occured";
        case HTTP_FILE_READ_ERR:         return "Reading of file failed";
        case HTTP_HEADER_CREATION_ERR:   return "Failed to create http header";
        case HTTP_FORBIDDEN:             return "Forbidden file path";
        case HTTP_VERSION_NOT_SUPPORTED: return "Http version not supported";
        default:                         return "Unknown http error";
    }
}

HttpResult http_handle_request(char* buf, HttpResponse* http_response) {
    HttpRequest http_request;
    HttpResult http_result;

    http_result = http_parse_request(buf, &http_request);
    if (http_result != HTTP_OK) goto handle_error;

    if (strstr(http_request.path, "..")) {
        http_result = HTTP_FORBIDDEN;
        goto handle_error;
    }

    char *file_to_serve = http_request.path;
    if (strcmp(file_to_serve, "/") == 0) {
        file_to_serve = "/index.html";
    }

    http_response->keep_alive = true;
    if (strstr(buf, "Connection: close") != NULL) {
        http_response->keep_alive = false;
    }

    http_result = http_get_mime_type(file_to_serve, http_response);
    if (http_result != HTTP_OK) goto handle_error;

    http_result = http_handle_file_request(file_to_serve, http_response);
    if (http_result != HTTP_OK) goto handle_error;

    http_status_from_result(http_result, http_response);

    printf("%s %s %s -> 200 OK\n",
        http_request.method, http_request.path, http_request.version
    );

    return HTTP_OK;

handle_error:
    http_status_from_result(http_result, http_response);
    snprintf(http_response->mime_type, MIME_TYPE_LEN, "%s", "text/html");
    http_response->keep_alive = false;
    http_response->content_length = 0;
    http_response->body = NULL;

    return http_result; // for sys error mesg in main
}

HttpResult http_serialize(HttpResponse* http_response) {
    int conn_len = http_response->keep_alive ? 
        snprintf(NULL, 0, "Connection: keep-alive\r\nKeep-Alive: timeout=%d, max=100\r\n", TIMEOUT) :
        snprintf(NULL, 0, "Connection: close\r\n");

    int header_len = snprintf(NULL, 0,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "%.*s" // ensures correct positions of conn str
        "\r\n",
        http_response->status_code,
        http_response->status_message,
        http_response->mime_type,
        http_response->content_length,
        0, "" // juts palce for conn str(filled by conn_len)
    ) + conn_len;

    http_response->response_size = header_len + http_response->content_length;
    http_response->response_buffer = malloc(http_response->response_size);
    if (http_response->response_buffer == NULL) {
        return HTTP_MALLOC_ERR;
    }

    char* ptr = http_response->response_buffer;
    int written = sprintf(ptr,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "X-Content-Type-Options: nosniff\r\n"
            "X-Frame-Options: DENY\r\n",
            http_response->status_code, http_response->status_message,
            http_response->mime_type, http_response->content_length);

    ptr += written;

    if (http_response->keep_alive) {
        ptr += sprintf(ptr, "Connection: keep-alive\r\nKeep-Alive: timeout=%d, max=100\r\n", TIMEOUT);
    } else {
        ptr += sprintf(ptr, "Connection: close\r\n");
    }
    
    ptr += sprintf(ptr, "\r\n");

    if (http_response->body) {
        memcpy(ptr, http_response->body, http_response->content_length);
    }

    return HTTP_OK;
}