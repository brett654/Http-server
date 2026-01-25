#ifndef HTTP_HPP
#define HTTP_HPP

#include <string_view>
#include <vector>
#include <expected>
#include <array>
#include <charconv>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
    MALFORMED_REQUEST  ,
    MIME_TYPE_NOT_FOUND,
    INVALID_FILE_PATH,
};

struct MimeEntry {
    std::string_view extension;
    std::string_view mime_type;
};

inline constexpr auto MIME_TYPES = std::to_array<MimeEntry>({
    {"css",  "text/css"},
    {"gif",  "image/gif"},
    {"htm",  "text/html"},
    {"html", "text/html"},
    {"jpeg", "image/jpeg"},
    {"jpg",  "image/jpeg"},
    {"js",   "application/javascript"},
    {"json", "application/json"},
    {"png",  "image/png"},
    {"txt",  "text/plain"}
});

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
        std::string_view status_message;

        std::vector<char> header_buf;

        int file_fd{-1};
        size_t file_size{0};
        int status_code{200};
        bool keep_alive{true};

        void clear() {
            header_buf.clear();
            status_message = {};
            status_code = 200;
            keep_alive = true;
            file_fd = -1;
            file_size = 0;
        }
    } response;

    Http();
    ~Http();

    std::expected<void, HttpError> parse(std::string_view buf);
    std::expected<void, HttpError> set_mime_type();
    std::expected<void, HttpError> set_file_meta();
    std::expected<void, HttpError> serialize_header();
    std::expected<void, HttpError> process_request(std::string_view buf);

    void set_error_status(HttpError err);
};

#endif