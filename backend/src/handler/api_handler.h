#pragma once

#include "../util/common.h"

namespace api_handler {

class ApiHandler {
public:
    virtual ~ApiHandler() = default;

    virtual StringResponse Handle(const StringRequest& request) const = 0;
};

class MapsApiHandler : public ApiHandler {
public:
    MapsApiHandler(app::Application& app);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;

    StringResponse GetMaps(unsigned version, bool keep_alive) const;

};

class MapByIdApiHandler : public ApiHandler {
public:
    MapByIdApiHandler(app::Application& app, const extra_data::Payload& payload);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;
    const extra_data::Payload& payload_;

    json::array GetJsonRoads(const model::Map::Roads& roads) const;

    json::array GetJsonBuildings(const model::Map::Buildings& buildings) const;

    json::array GetJsonOffices(const model::Map::Offices& offices) const;

    StringResponse GetMapById(const std::string& map_id, unsigned version, bool keep_alive) const;

};

class JoinGameApiHandler : public ApiHandler {
public:
    JoinGameApiHandler(app::Application& app);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;

    StringResponse JoinGame(unsigned version, bool keep_alive, const json::object& request_body) const;

};

class PlayersApiHandler : public ApiHandler {
public:
    PlayersApiHandler(app::Application& app);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;

    StringResponse GetPlayerList(unsigned version, bool keep_alive, const std::string& credentials) const;

};

class GameStateApiHandler : public ApiHandler {
public:
    GameStateApiHandler(app::Application& app);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;

    json::object GetJsonGameState(const app::GameState& player_info) const;

    StringResponse GetGameState(unsigned version, bool keep_alive, const std::string& credentials) const;

};

class PlayerActionApiHandler : public ApiHandler {
public:
    PlayerActionApiHandler(app::Application& app);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;

    StringResponse SetPlayerAction(unsigned version, bool keep_alive, const std::string& credentials, const json::object& request_body) const;

};

class TickApiHandler : public ApiHandler {
public:
    TickApiHandler(app::Application& app, bool is_state_file_set, bool is_save_state_period_set, bool is_tick_period_set);

    StringResponse Handle(const StringRequest& request) const override;

private:
    app::Application& app_;
    bool is_state_file_set_;
    bool is_save_state_period_set_;
    bool is_tick_period_set_;

    StringResponse UpdateGameState(unsigned version, bool keep_alive, int delta) const;

};

class ApiHandlerFactory {
public:
    virtual ~ApiHandlerFactory() = default;

    virtual std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const = 0;
};

class MapsApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class MapByIdApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class JoinGameApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class PlayersApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class GameStateApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class PlayerActionApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class TickApiHandlerFactory : public ApiHandlerFactory {
public:
    std::shared_ptr<ApiHandler> CreateApiHandler(ApiHandlerParams& params) const override;
};

class ApiHandlerManager {
public:
    ApiHandlerManager(ApiHandlerParams& params);

    StringResponse HandleApiRequest(const StringRequest& request);

    void Tick(int delta);

private:
    ApiHandlerParams& params_;
    std::unordered_map<std::string, std::shared_ptr<ApiHandlerFactory>> endpoint_to_factory_;
};

}  // namespace api_handler
