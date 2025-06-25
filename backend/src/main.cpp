#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include "loader/json_loader.h"
#include "handler/request_handler.h"
#include "serialization/model_serialization.h"

namespace sys = boost::system;
namespace fs = std::filesystem;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_entry;
    log_entry["timestamp"] = to_iso_extended_string(*rec[timestamp]);
    log_entry["data"] = *rec[additional_data];
    log_entry["message"] = *rec[expr::smessage];
    strm << json::serialize(log_entry);
}

void Init() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::clog,
        keywords::format = &Formatter,
        keywords::auto_flush = true
    );
}

void LogIn(const json::object& custom_data) {
    if (auto it = custom_data.find("URI"); it != custom_data.end()) {
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "request received"sv;
    } else if (auto it = custom_data.find("response_time"); it != custom_data.end()) {
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "response sent"sv;
    } else if (auto it = custom_data.find("text"); it != custom_data.end()) {
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "error"sv;
    }
}

struct Args {
    unsigned int tick_period;
    std::string config_file_path;
    std::string root;
    bool random_positions;
    std::string state_file;
    unsigned int save_state_period;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<unsigned int>(&args.tick_period)->default_value(0), "set tick period in milliseconds")
        ("config-file,c", po::value<std::string>(&args.config_file_path), "set config file path")
        ("www-root,w", po::value<std::string>(&args.root), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.random_positions), "spawn dogs at random positions")
        ("state-file", po::value<std::string>(&args.state_file), "set path to the state file")
        ("save-state-period", po::value<unsigned int>(&args.save_state_period), "set period for automatic state saving in milliseconds");
    // variables_map stores option values after parsing
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.contains("help"s) && vm.contains("h"s)) {
        std::cout << desc;
        return std::nullopt;
    }
    return args;
}

void Save(const app::Application& app, const std::string& filename) {
    std::string temp_filename = filename + "_tmp";
    {
        std::ofstream file(temp_filename);
        if (file.is_open()) {
            boost::archive::text_oarchive ar(file);
            serialization::ApplicationRepr app_repr{app};
            ar << app_repr;
        } else {
            throw std::runtime_error("Unable to open temporary file for writing");
        }
    }
    fs::rename(temp_filename, filename);
}

void Load(app::Application& app, const std::string& filename) {
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        boost::archive::text_iarchive ar{file};
        serialization::ApplicationRepr restored_app_repr;
        ar >> restored_app_repr;
        restored_app_repr.Restore(app);
    }
}

namespace {

template <typename Fn>
void RunAsyncOperations(unsigned int n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> threads;
    threads.reserve(n - 1);
    while (--n) {
        threads.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            if (args->config_file_path.empty() && args->root.empty()) {
                std::cerr << "Usage: game_server <game-config-json> <static-path>"sv << std::endl;
                return EXIT_FAILURE;
            }

            Init();
            auto data_collection = [](const json::object& custom_data) {
                LogIn(custom_data);
            };

            // Download the map from the file and build a game model
            extra_data::Payload payload;
            model::Game game = json_loader::LoadGame(args->config_file_path, payload);

            app::Application app{std::make_unique<model::Game>(game), args->random_positions};
            // Initialize io_context
            const unsigned int num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);
            bool is_state_file_set = args->state_file.empty();
            // When the server starts with the path to an existing status file, it should restore this state
            if (!is_state_file_set && fs::exists(args->state_file)) {
                try {
                    Load(app, args->state_file);
                } catch (const std::runtime_error& e) {
                    json::value custom_data{{"exception"s, e.what()}};
                    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                            << "server exited"sv;
                    return EXIT_FAILURE;
                }
            }
            // The lambda function will be called whenever the Application sends a tick signal
            sig::scoped_connection conn = app.DoOnTick([total = 0ms, &args, &app](milliseconds delta) mutable {
                total += delta;
                if (total >= milliseconds(args->save_state_period)) {
                    Save(app, args->state_file);
                    total = 0ms;
                }
            });
            // Adding an asynchronous signal handler SIGINT и SIGTERM
            // We subscribe to the signals and shut down the server when we receive them
            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc, &args, &app](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {
                    json::value custom_data{{"code"s, signal_number}};
                    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                            << "server exited"sv;

                    ioc.stop();
                } else {
                    json::value custom_data{{"exception"s, ec.what()}};
                    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                            << "server exited"sv;
                }
            });

            bool is_save_state_period_set = args->save_state_period != 0;
            bool is_tick_period_set = args->tick_period != 0;
            ApiHandlerParams params{payload, app, is_state_file_set, is_save_state_period_set, is_tick_period_set};

            // Creating an API Request Handler Manager
            api_handler::ApiHandlerManager api_handler_manager{params};

            auto api_strand = net::make_strand(ioc);
            if (params.is_tick_period_set) {
                auto ticker = std::make_shared<Ticker>(api_strand, milliseconds(args->tick_period),
                    [&api_handler_manager](milliseconds delta) {
                        api_handler_manager.Tick(static_cast<int>(delta.count()));
                });
                ticker->Start();
            }

            fs::path static_files_root{args->root};
            detail::DurationMeasure measure;
            // Creating an HTTP request handler
            auto handler = std::make_shared<http_handler::RequestHandler>(api_handler_manager, static_files_root, api_strand, measure, data_collection);
            const auto address = net::ip::make_address("0.0.0.0");
            constexpr net::ip::port_type port = 8080;
            json::object custom_data;
            custom_data.emplace("port", static_cast<int>(port));
            custom_data.emplace("address", address.to_string());
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                    << "server started"sv;
            // Start the HTTP request handler by delegating them to the request handler
            http_server::ServeHttp(ioc, {address, port}, data_collection, [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            });
            // Starting processing of asynchronous operations
            RunAsyncOperations(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });
            // All asynchronous operations are completed, save the server status to a file
            if (!params.is_state_file_set) {
                app.Tick(milliseconds(args->save_state_period));
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        json::value custom_data{{"exception"s, ex.what()}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "server exited"sv;
        return EXIT_FAILURE;
    } catch (...) {
        json::value custom_data{{"code"s, EXIT_FAILURE}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data)
                                << "server exited"sv;
        return EXIT_FAILURE;
    }
}
