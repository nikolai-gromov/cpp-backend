#include "http_server.h"

#include <iostream>

#include <boost/asio/dispatch.hpp>

namespace http_server {

SessionBase::SessionBase(tcp::socket&& socket, std::function<void(const json::object&)> data_collection)
    : stream_(std::move(socket))
    , data_collection_(data_collection) {
}

void SessionBase::Run() {
    // Call the Read method using the executor of the stream_ object.
    // Thus, all operations with stream_ will be performed using its executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    using namespace std::literals;
    // Clear the request from the previous value (the Read method may be called multiple times)
    request_ = {};
    stream_.expires_after(30s);
    // Read request_ from stream_, using buffer_ to store the read data
    http::async_read(stream_, buffer_, request_,
                     // After the operation is complete, the OnRead method will be called
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        // Normal situation - the client closed the connection
        return Close();
    }
    if (ec) {
        json::object custom_data;
        custom_data.emplace("code", std::to_string(ec.value()));
        custom_data.emplace("text", ec.message());
        custom_data.emplace("where", "read");
        data_collection_(custom_data);

        return;
    }

    json::object custom_data;
    custom_data.emplace("ip", stream_.socket().remote_endpoint().address().to_string());
    custom_data.emplace("URI", std::string{request_.target()});
    custom_data.emplace("method", std::string{request_.method_string()});
    data_collection_(custom_data);

    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        json::object custom_data;
        custom_data.emplace("code", std::to_string(ec.value()));
        custom_data.emplace("text", ec.message());
        custom_data.emplace("where", "close");
        data_collection_(custom_data);

        return;
    }
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        json::object custom_data;
        custom_data.emplace("code", std::to_string(ec.value()));
        custom_data.emplace("text", ec.message());
        custom_data.emplace("where", "write");
        data_collection_(custom_data);

        return;
    }

    if (close) {
        // The semantics of the response require closing the connection
        return Close();
    }

    // Read the next request
    Read();
}

}  // namespace http_server