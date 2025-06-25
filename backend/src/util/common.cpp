#include "common.h"

namespace detail {

void DurationMeasure::StartMeasurement() {
    std::lock_guard<std::mutex> lock(mutex_);
    start_ts_ = std::chrono::system_clock::now();
}

std::int64_t DurationMeasure::GetDuration() {
    std::lock_guard<std::mutex> lock(mutex_);
    end_ts_ = std::chrono::system_clock::now();
    duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts_ - start_ts_).count();
    return duration_;
}

}  // namespace detail

ApiHandlerParams::ApiHandlerParams(const extra_data::Payload& pl,
                                   app::Application& app,
                                   bool is_state_file,
                                   bool is_save_state_period,
                                   bool is_tick_period)
    : payload{pl}
    , ref_app{app}
    , is_state_file_set{is_state_file}
    , is_save_state_period_set{is_save_state_period}
    , is_tick_period_set{is_tick_period} {
}

Ticker::Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
    : strand_{strand}
    , period_{period}
    , handler_{std::move(handler)} {
}

void Ticker::Start() {
    net::dispatch(strand_, [self = shared_from_this()] {
        self->last_tick_ = Clock::now();
        self->ScheduleTick();
    });
}

void Ticker::ScheduleTick() {
    assert(strand_.running_in_this_thread());
    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
        self->OnTick(ec);
    });
}

void Ticker::OnTick(sys::error_code ec) {
    using namespace std::chrono;
    assert(strand_.running_in_this_thread());

    if (!ec) {
        auto this_tick = Clock::now();
        auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
        last_tick_ = this_tick;
        try {
            handler_(delta);
        } catch (...) {
        }
        ScheduleTick();
    }
}

StringResponse MakeBadRequestError(Version version, bool keep_alive, const std::string& code, const std::string& message) {
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::bad_request);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    json::object json_error;
    json_error["code"] = code;
    json_error["message"] = message;
    response.body() = json::serialize(json_error);
    response.content_length(response.body().size());
    return response;
}

StringResponse MakeMethodNotAllowedError(Version version, bool keep_alive, const std::string& allow, const std::string& code, const std::string& message) {
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::method_not_allowed);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    response.set(http::field::allow, allow);
    json::object json_error;
    json_error["code"] = code;
    json_error["message"] = message;
    response.body() = json::serialize(json_error);
    response.content_length(response.body().size());
    return response;
}

StringResponse MakeNotFoundError(Version version, bool keep_alive, const std::string& code, const std::string& message) {
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::not_found);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    json::object json_error;
    json_error["code"] = code;
    json_error["message"] = message;
    response.body() = json::serialize(json_error);
    response.content_length(response.body().size());
    return response;
}

StringResponse MakeUnauthorizedError(Version version, bool keep_alive, const std::string& code, const std::string& message) {
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::unauthorized);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    json::object json_error;
    json_error["code"] = code;
    json_error["message"] = message;
    response.body() = json::serialize(json_error);
    response.content_length(response.body().size());
    return response;
}

bool IsGetOrHeadMethod(http::verb method) {
    return method == http::verb::get || method == http::verb::head;
}

bool IsPostMethod(http::verb method) {
    return method == http::verb::post;
}

std::optional<StringResponse> CheckGetOrHeadMethod(Version version, bool keep_alive, http::verb method) {
    if (IsGetOrHeadMethod(method)) {
        return std::nullopt;
    }
    return MakeMethodNotAllowedError(version, keep_alive, "GET, HEAD", "invalidMethod", "Invalid method");
}

std::optional<StringResponse> CheckPostMethod(Version version, bool keep_alive, http::verb method) {
    if (IsPostMethod(method)) {
        return std::nullopt;
    }
    return MakeMethodNotAllowedError(version, keep_alive, "POST", "invalidMethod", "Invalid method");
}