#include "../include/http_server/http.hpp"

Http::Http() :
    request{}, response{} {
    response.header_buf.reserve(512);
}

Http::~Http() = default;

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