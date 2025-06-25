#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <optional>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


#include "extra_data.h"
#include "../app/app.h"
#include "../server/http_server.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace json = boost::json;
namespace sig = boost::signals2;
using namespace std::literals;
using Version = unsigned;
using milliseconds = std::chrono::milliseconds;

// A request whose body is represented as a string
using StringRequest = http::request<http::string_body>;
// The response, the body of which is represented as a string
using StringResponse = http::response<http::string_body>;
// The response, the body of which is presented as a file
using FileResponse = http::response<http::file_body>;

namespace detail {

class DurationMeasure {
public:
    void StartMeasurement();

    std::int64_t GetDuration();

private:
    std::chrono::system_clock::time_point start_ts_;
    std::chrono::system_clock::time_point end_ts_;
    std::int64_t duration_;
    std::mutex mutex_;
};

}  // namespace detail

struct ApiHandlerParams {
    const extra_data::Payload& payload;
    app::Application& ref_app;
    bool is_state_file_set;
    bool is_save_state_period_set;
    bool is_tick_period_set;

    ApiHandlerParams(const extra_data::Payload& pl,
                     app::Application& app,
                     bool is_state_file,
                     bool is_save_state_period,
                     bool is_tick_period);
};

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(milliseconds delta)>;

    Ticker(Strand strand, milliseconds period, Handler handler);

    void Start();

private:
    void ScheduleTick();

    void OnTick(sys::error_code ec);

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    Clock::time_point last_tick_;
};

StringResponse MakeBadRequestError(Version version, bool keep_alive, const std::string& code, const std::string& message);

StringResponse MakeMethodNotAllowedError(Version version, bool keep_alive, const std::string& allow, const std::string& code, const std::string& message);

StringResponse MakeNotFoundError(Version version, bool keep_alive, const std::string& code, const std::string& message);

StringResponse MakeUnauthorizedError(Version version, bool keep_alive, const std::string& code, const std::string& message);

bool IsGetOrHeadMethod(http::verb method);

bool IsPostMethod(http::verb method);

std::optional<StringResponse> CheckGetOrHeadMethod(Version version, bool keep_alive, http::verb method);

std::optional<StringResponse> CheckPostMethod(Version version, bool keep_alive, http::verb method);