#include "request_handler.h"

#include <sstream>
#include <iomanip>

namespace http_handler {

RequestHandler::RequestHandler(api_handler::ApiHandlerManager& api_handler_manager,
                               const fs::path& root,
                               RequestHandler::Strand api_strand,
                               detail::DurationMeasure& measure,
                               std::function<void(const json::object&)> data_collection)
    : api_handler_manager_{api_handler_manager}
    , root_{root}
    , measure_{measure}
    , api_strand_{api_strand}
    , data_collection_{data_collection} {
}

std::string RequestHandler::UrlDecode(const std::string& path) {
    std::string decoded_path;
    std::istringstream iss(path);
    char c;
    int value;
    while (iss.get(c)) {
        if (c == '%' && iss >> std::hex >> value) {
            decoded_path += static_cast<char>(value);
        } else if (c == '+') {
            decoded_path += ' ';
        } else {
            decoded_path += c;
        }
    }
    return decoded_path;
}

bool RequestHandler::IsSubPath(const std::string& file_path) {
    // We bring both paths to the canonical form (without . and ..)
    fs::path path = fs::weakly_canonical(fs::path{file_path});
    fs::path base = fs::weakly_canonical(root_);
    // We check that all base components are contained inside the path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::string RequestHandler::GetFileExtension(const std::string& file_path) {
    std::string file_extension = fs::path(file_path).extension();
    return file_extension;
}

std::string RequestHandler::GetMineType(const std::string& file_extension) {
    static const std::unordered_map<std::string, std::string> mine_types = {
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}
    };

    std::string lower_case_extension = file_extension;
    std::transform(lower_case_extension.begin(), lower_case_extension.end(), lower_case_extension.begin(),
                    [](unsigned char c){ return std::tolower(c); }
                   );

    auto it = mine_types.find(lower_case_extension);
    if (it != mine_types.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

FileResponse RequestHandler::GetResponseWithFile(const std::string& file_path) {
    FileResponse response_file;
    response_file.version(11);
    response_file.result(http::status::ok);
    response_file.insert(http::field::content_type, GetMineType(GetFileExtension(file_path)));
    response_file.set(http::field::cache_control, "no-cache");

    http::file_body::value_type file;

    sys::error_code ec;
    file.open(file_path.c_str(), beast::file_mode::read, ec);

    if (!ec) {
        response_file.body() = std::move(file);
        response_file.prepare_payload();
    }
    return response_file;
}

StringResponse RequestHandler::ReportServerError(unsigned version, bool keep_alive) const {
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::bad_request);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    json::object json_error;
    json_error["code"] = "badRequest";
    json_error["message"] = "Bad request";
    response.body() = json::serialize(json_error);
    return response;
}

RequestHandler::FileRequestResult RequestHandler::HandleFileRequest(const StringRequest& req, unsigned version, bool keep_alive) {
    std::string req_str{req.target()};
    std::string decoded_url_str = UrlDecode(req_str);
    std::string file_path{std::string(root_) + decoded_url_str};
    auto method = req.method();

    FileResponse response_file;
    if ((!req_str.starts_with("/api"sv)) && IsGetOrHeadMethod(method)) {
        if (req_str == "/"sv) {
            response_file = std::move(GetResponseWithFile({std::string(root_) + req_str + "index.html"s}));
        } else if (IsSubPath(file_path)) {
            response_file = std::move(GetResponseWithFile(file_path));
        } else {
            StringResponse response;
            response.version(req.version());
            response.keep_alive(req.keep_alive());
            response.set(http::field::content_type, "text/plain");
            response.set(http::field::cache_control, "no-cache");
            response.result(http::status::bad_request);
            response.body() = "400 Bad request";
            return response;
        }

        if (response_file.body().size() != 0) {
            return response_file;
        } else {
            StringResponse response;
            response.version(req.version());
            response.keep_alive(req.keep_alive());
            response.set(http::field::content_type, "text/plain");
            response.set(http::field::cache_control, "no-cache");
            response.result(http::status::not_found);
            response.body() = "404 Not Found";
            return response;
        }
    } else {
        return ReportServerError(version, keep_alive);
    }
}

}  // namespace http_handler
