#pragma once

#include <iostream>
#include <functional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

namespace http_server {

namespace net = boost::asio;
namespace sys = boost::system;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = net::ip::tcp;
using namespace std::literals;

class SessionBase {
public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run();

protected:
    using HttpRequest = http::request<http::string_body>;

    explicit SessionBase(tcp::socket&& socket, std::function<void(const json::object&)> data_collection);

    ~SessionBase() = default;

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        // The write is performed asynchronously, so we move response to the heap
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

        auto self = GetSharedThis();
        http::async_write(stream_, *safe_response,
                          [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                              self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                          });
    }

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::function<void(const json::object&)> data_collection_;
    HttpRequest request_;

private:
    void Read();

    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);

    void Close();

    void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);

    virtual void HandleRequest(HttpRequest&& request) = 0;

    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, std::function<void(const json::object&)> data_collection, Handler&& request_handler)
        : SessionBase(std::move(socket), data_collection)
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    RequestHandler request_handler_;

private:
    std::shared_ptr<SessionBase> GetSharedThis() override {
        return this->shared_from_this();
    }

    void HandleRequest(HttpRequest&& request) override {
        // Capture a smart pointer to the current Session object in the lambda,
        // to extend the lifetime of the session until the lambda is called.
        // A generic lambda function is used, capable of accepting a response of any type
        request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
            self->Write(std::move(response));
        });
    }
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, std::function<void(const json::object&)> data_collection, Handler&& request_handler)
        : ioc_(ioc)
        // The handlers for asynchronous operations of acceptor_ will be called in its strand
        , acceptor_(net::make_strand(ioc))
        , data_collection_(data_collection)
        , request_handler_(std::forward<Handler>(request_handler)) {
        // Open the acceptor using the protocol (IPv4 or IPv6) specified in the endpoint
        acceptor_.open(endpoint.protocol());

        // After closing a TCP connection, the socket may be considered busy for a while,
        // so that computers can exchange terminating data packets.
        // However, this may prevent reopening the socket in a half-closed state.
        // The reuse_address flag allows the socket to be opened when it is "half-closed"
        acceptor_.set_option(net::socket_base::reuse_address(true));
        // Bind the acceptor to the address and port of the endpoint
        acceptor_.bind(endpoint);
        // Put the acceptor into a state where it can accept new connections
        // This allows new connections to be queued for acceptance
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void Run() {
        DoAccept();
    }

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::function<void(const json::object&)> data_collection_;
    RequestHandler request_handler_;

private:
    void DoAccept() {
        acceptor_.async_accept(
            // Pass a sequential executor where the handlers for
            // asynchronous socket operations will be called
            net::make_strand(ioc_),
            // Using bind_front_handler, we create a handler bound to the OnAccept method
            // of the current object.
            // Since Listener is a template class, we need to inform the compiler that
            // shared_from_this is a class method, not a free function.
            // To do this, we call it using this
            // This bind_front_handler call is equivalent to
            // namespace ph = std::placeholders;
            // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    // The socket::async_accept method will create a socket and pass it to OnAccept
    void OnAccept(sys::error_code ec, tcp::socket socket) {
        using namespace std::literals;

        if (ec) {
            json::object custom_data;

            custom_data.emplace("code", std::to_string(ec.value()));
            custom_data.emplace("text", ec.message());
            custom_data.emplace("where", "accept");

            data_collection_(custom_data);
            return;
        }

        // Asynchronously handle the session
        AsyncRunSession(std::move(socket));

        // Accept a new connection
        DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), data_collection_, request_handler_)->Run();
    }
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, std::function<void(const json::object&)> data_collection, RequestHandler&& handler) {
    // Using decay_t, we will exclude references from the RequestHandler type,
    // so that the Listener stores the RequestHandler by value
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, data_collection, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server