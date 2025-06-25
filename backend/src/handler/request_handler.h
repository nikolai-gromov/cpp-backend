#pragma once

#include <variant>

#include "api_handler.h"

namespace http_handler {

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    RequestHandler(api_handler::ApiHandlerManager& api_handler_manager,
                   const fs::path& root,
                   Strand api_strand,
                   detail::DurationMeasure& measure,
                   std::function<void(const json::object&)> data_collection);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto version = req.version();
        auto keep_alive = req.keep_alive();
        try {
            if (req.target().starts_with("/api/v1"sv)) {
                auto handle = [self = shared_from_this(), send,
                               req = std::forward<decltype(req)>(req), version, keep_alive] {
                    try {
                        json::object custom_data;
                        self->measure_.StartMeasurement();
                        assert(self->api_strand_.running_in_this_thread());
                        auto response = std::move(self->api_handler_manager_.HandleApiRequest(req));
                        custom_data.emplace("response_time", self->measure_.GetDuration());
                        custom_data.emplace("code", response.result_int());
                        custom_data.emplace("content_type", std::string{response[http::field::content_type]});
                        self->data_collection_(custom_data);
                        return send(response);
                    } catch (const std::exception& e) {
                        json::object custom_data;
                        self->measure_.StartMeasurement();
                        auto response = std::move(self->ReportServerError(version, keep_alive));
                        custom_data.emplace("response_time", self->measure_.GetDuration());
                        custom_data.emplace("code", response.result_int());
                        custom_data.emplace("content_type", std::string{response[http::field::content_type]});
                        self->data_collection_(custom_data);
                        return send(response);
                    }
                };

                return net::dispatch(api_strand_, handle);
            }
            return std::visit(
                [&](auto&& result) {
                    json::object custom_data;
                    measure_.StartMeasurement();
                    custom_data.emplace("response_time", measure_.GetDuration());
                    custom_data.emplace("code", result.result_int());
                    custom_data.emplace("content_type", std::string{result[http::field::content_type]});
                    data_collection_(custom_data);
                    send(std::forward<decltype(result)>(result));
                },
                HandleFileRequest(req, version, keep_alive));
        } catch (...) {
            json::object custom_data;
            measure_.StartMeasurement();
            auto response = ReportServerError(version, keep_alive);
            custom_data.emplace("response_time", measure_.GetDuration());
            custom_data.emplace("code", response.result_int());
            custom_data.emplace("content_type", std::string{response[http::field::content_type]});
            data_collection_(custom_data);
            send(response);
        }
    }

private:
    api_handler::ApiHandlerManager& api_handler_manager_;
    fs::path root_;
    Strand api_strand_;
    detail::DurationMeasure& measure_;
    std::function<void(const json::object&)> data_collection_;

    using FileRequestResult = std::variant<StringResponse, FileResponse>;

    std::string UrlDecode(const std::string& path);

    bool IsSubPath(const std::string& file_path);

    std::string GetFileExtension(const std::string& file_path);

    std::string GetMineType(const std::string& file_extension);

    FileResponse GetResponseWithFile(const std::string& file_path);

    StringResponse ReportServerError(unsigned version, bool keep_alive) const;

    FileRequestResult HandleFileRequest(const StringRequest& req, unsigned version, bool keep_alive);
};

}  // namespace http_handler
