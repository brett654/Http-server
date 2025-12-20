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

HttpResult http_parse_request(const char* buf, HttpRequest* http_request) {
    int scan_result = sscanf(buf,
        "%8s %256s %16s",
        http_request->method,
        http_request->path,
        http_request->version
    );

    if (scan_result < 3) {
        return HTTP_PARSE_ERR;
    }

    if (strncmp(http_request->method, "GET", 3) != 0) {
        return HTTP_METHOD_NOT_SUPPORTED;
    }

    return HTTP_OK;
}

HttpResult http_get_mime_type(const char* file_path, HttpResponse* http_response) {
    char* dot = strrchr(file_path, '.');
    if (!dot) {
        strcpy(http_response->mime_type, "text/plain");
        return HTTP_OK;
    }

    char* ext = dot + 1;
    
    if (strcmp(ext, "html") == 0) {
        strcpy(http_response->mime_type, "text/html");
    } else {
        strcpy(http_response->mime_type, "text/plain");
    }

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
    long fsize = ftell(file);
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
        case HTTP_FILE_NOT_FOUND:
            http_response->status_code = 404;
            strcpy(http_response->status_message, "Not Found");
            break;
        case HTTP_METHOD_NOT_SUPPORTED:
            http_response->status_code = 405;
            strcpy(http_response->status_message, "Method Not Allowed");
            break;
        case HTTP_PARSE_ERR:
            http_response->status_code = 400;
            strcpy(http_response->status_message, "Bad Request");
            break;
        case HTTP_MALLOC_ERR:
        case HTTP_FILE_READ_ERR:
        default:
            http_response->status_code = 500;
            strcpy(http_response->status_message, "Internal Server Error");
            break;
    }
}

const char* http_strerror(HttpResult http_result) {
    switch (http_result) {
        case HTTP_OK:                   return "Success";
        case HTTP_PARSE_ERR:            return "Failed to parse request";
        case HTTP_METHOD_NOT_SUPPORTED: return "Http method not supported";
        case HTTP_FILE_NOT_FOUND:       return "File not found";
        case HTTP_MALLOC_ERR:           return "Malloc error occured";
        case HTTP_FILE_READ_ERR:        return "Reading of file failed";
        case HTTP_HEADER_CREATION_ERR:  return "Failed to create http header";
        default:                        return "Unknown http error";
    }
}

HttpResult http_handle_request(const char* buf, HttpResponse* http_response) {
    HttpRequest http_request;
    HttpResult http_result;

    http_result = http_parse_request(buf, &http_request);
    if (http_result != HTTP_OK) goto handle_error;
    printf("Parsed request:\n%s %s %s\n", http_request.method, http_request.path, http_request.version);
    
    char *file_to_serve = http_request.path;
    if (strcmp(file_to_serve, "/") == 0) {
        file_to_serve = "/index.html";
    }

    http_result = http_get_mime_type(file_to_serve, http_response);
    if (http_result != HTTP_OK) goto handle_error;

    http_result = http_handle_file_request(file_to_serve, http_response);
    if (http_result != HTTP_OK) goto handle_error;

    http_status_from_result(http_result, http_response);
    return HTTP_OK;

handle_error:
    // ERROR CASE: Map the internal error to the public HTTP status
    http_status_from_result(http_result, http_response);
    return http_result; // for sys error mesg in main
}

HttpResult http_serialize(HttpResponse* http_response) {
    char header_buf[1024];

    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        http_response->status_code,
        http_response->status_message,
        http_response->mime_type,
        http_response->content_length
    );

    if (header_len < 0 || header_len >= sizeof(header_buf)) {
        return HTTP_HEADER_CREATION_ERR; 
    }

    http_response->response_size = header_len + http_response->content_length;
    http_response->response_buffer = malloc(http_response->response_size);

    if (http_response->response_buffer == NULL) {
        return HTTP_MALLOC_ERR;
    }

    memcpy(http_response->response_buffer, header_buf, header_len);
    if (http_response->body) {
        memcpy(http_response->response_buffer + header_len, http_response->body, http_response->content_length);
    }
    
    return HTTP_OK;
}