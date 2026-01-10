#ifndef HTTP_HPP
#define HTTP_HPP

#include <string_view>
#include <vector>
#include <expected>

enum class HttpError {
    PARSE_ERR,
    METHOD_NOT_SUPPORTED,
    FILE_NOT_FOUND,
    MALLOC_ERR,
    FILE_READ_ERR,
    HEADER_CREATION_ERR,
    FORBIDDEN,
    VERSION_NOT_SUPPORTED,
    URI_TOO_LONG,
    MALFORMED_REQUEST    
};

enum class HttpResult {
    HTTP_OK = 0,
    HTTP_PARSE_ERR,
    HTTP_METHOD_NOT_SUPPORTED,
    HTTP_FILE_NOT_FOUND,
    HTTP_MALLOC_ERR,
    HTTP_FILE_READ_ERR,
    HTTP_HEADER_CREATION_ERR,
    HTTP_FORBIDDEN,
    HTTP_VERSION_NOT_SUPPORTED,
    HTTP_URI_TOO_LONG,    
};

class Http {
public:
    struct Request {
        std::string_view method;
        std::string_view path;
        std::string_view version;

        void clear() { *this = Request{}; }
    } request;

    struct Response {
        std::string_view mime_type{"text/plain"};
        std::string_view body;

        std::vector<char> header_buf;

        int status_code{200};
        bool keep_alive{true};

        void clear() {
            header_buf.clear();
            body = {};
            status_code = 200;
            keep_alive = true;
        }
    } response;

    Http();
    ~Http();

    std::expected<void, HttpError> parse(std::string_view buf);
    std::expected<void, HttpError> mime_type();
};

#endif