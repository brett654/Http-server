#include "../include/http_server/http.hpp"

Http::Http() :
    request{}, response{} {
    response.header_buf.reserve(512);
}

Http::~Http() {
    request.clear();
    response.clear();
}

std::expected<void, HttpError> Http::parse(std::string_view buf) {
    size_t m_end = buf.find(' ');
    if (m_end == std::string_view::npos)
        return std::unexpected(HttpError::MALFORMED_REQUEST);

    request.method = buf.substr(0, m_end);
    if (request.method != "GET")
        return std::unexpected(HttpError::METHOD_NOT_SUPPORTED);

    size_t p_start = m_end + 1;
    size_t p_end = buf.find(' ', p_start);
    if (p_end == std::string_view::npos)
        return std::unexpected(HttpError::MALFORMED_REQUEST);
    request.path = buf.substr(p_start, p_end - p_start);

    size_t v_start = p_end + 1;
    size_t v_end = buf.find("\r\n", v_start);
    if (v_end == std::string_view::npos)
        return std::unexpected(HttpError::MALFORMED_REQUEST);  
    request.version = buf.substr(v_start, v_end - v_start);

    return {};
}

std::expected<void, HttpError> Http::set_mime_type() {
    const size_t dot_pos = request.path.find_last_of('.');
    if (dot_pos == std::string_view::npos)
        return std::unexpected(HttpError::INVALID_FILE_PATH);

    const std::string_view ext = request.path.substr(dot_pos + 1);

    for (size_t i = 0; i < MIME_TYPES.size(); i++) {
        if (ext == MIME_TYPES[i].extension) {
            response.mime_type = MIME_TYPES[i].mime_type;
            return {};
        }
    }

    response.mime_type = "application/octet-stream";
    return std::unexpected(HttpError::MIME_TYPE_NOT_FOUND);
}

std::expected<void, HttpError> Http::set_file_meta() {
    int fd = open(request.path.data(), O_RDONLY | O_CLOEXEC);

    if (fd == -1) {
        if (errno == ENOENT) return std::unexpected(HttpError::FILE_NOT_FOUND);
        if (errno == EACCES) return std::unexpected(HttpError::FORBIDDEN);
        return std::unexpected(HttpError::FILE_READ_ERR);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return std::unexpected(HttpError::FILE_READ_ERR);
    }

    response.file_fd = fd;
    response.file_size = st.st_size;

    return {};
}

std::expected<void, HttpError> Http::serialize_header() {
    auto& v = response.header_buf;

    auto append_sv = [&v](std::string_view sv) {
        v.insert(v.end(), sv.begin(), sv.end());
    };

    auto append_int = [&v](int val) {
        char buf[20];
        auto [end_ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val); 
        v.insert(v.end(), buf, end_ptr);       
    };

    // Status Line
    append_sv("HTTP/1.1 ");
    append_int(response.status_code);
    append_sv(" ");
    append_sv(response.status_message);
    append_sv("\r\n");

    // Mandatory Headers
    append_sv("Content-Type: ");
    append_sv(response.mime_type);
    append_sv("\r\nContent-Length: ");
    append_int(response.file_size);

    // Header End
    append_sv("\r\n\r\n");

    return {};
}

std::expected<void, HttpError> Http::process_request(std::string_view buf) {
    auto parse_result = parse(buf);
    if (!parse_result) {
        return std::unexpected(parse_result.error());
    }

    auto mime_type_result = set_mime_type();
    if (!mime_type_result) {
        return std::unexpected(mime_type_result.error());
    } // Will implement error handling later

    auto file_meta_result = set_file_meta();
    if (!file_meta_result) {
        return std::unexpected(file_meta_result.error());
    }

    response.status_code = 200;
    response.status_message = "OK";

    auto serialize_result = serialize_header();
    if (!serialize_result) {
        return std::unexpected(serialize_result.error());
    }

    return {};
}

void Http::set_error_status(HttpError err) {
    switch (err) {
        case HttpError::MALFORMED_REQUEST:
        case HttpError::PARSE_ERR:
            response.status_code = 400;
            response.status_message = "Bad Request";
            break;        
        case HttpError::URI_TOO_LONG:
            response.status_code = 414;
            response.status_message = "URI Too Long";
            break;
        case HttpError::FORBIDDEN:
        case HttpError::INVALID_FILE_PATH:
            response.status_code = 403;
            response.status_message = "Forbidden";
            break;
        case HttpError::FILE_NOT_FOUND:
        case HttpError::MIME_TYPE_NOT_FOUND:
            response.status_code = 404;
            response.status_message = "Not Found";
            break;
        case HttpError::METHOD_NOT_SUPPORTED:
            response.status_code = 405;
            response.status_message = "Method Not Allowed";
            break;          
        case HttpError::VERSION_NOT_SUPPORTED:
            response.status_code = 505;
            response.status_message = "HTTP Version Not Supported";
            break;
        case HttpError::MALLOC_ERR:
        case HttpError::FILE_READ_ERR:
        case HttpError::HEADER_CREATION_ERR:
        default:
            response.status_code = 500;
            response.status_message = "Internal Server Error";
            break;
    }
}