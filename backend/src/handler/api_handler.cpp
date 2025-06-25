#include "api_handler.h"

namespace api_handler {

MapsApiHandler::MapsApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse MapsApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (auto error = CheckGetOrHeadMethod(version, keep_alive, method); error) {
        return *error;
    }
    return GetMaps(version, keep_alive);
}

StringResponse MapsApiHandler::GetMaps(unsigned version, bool keep_alive) const {
    const std::vector<model::Map>& maps = app_.GetGame()->GetMaps();
    boost::json::array json_maps;
    for (const auto& map : maps) {
        boost::json::object json_map;
        json_map["id"] = boost::json::value_from(*map.GetId());
        json_map["name"] = map.GetName();
        json_maps.push_back(json_map);
    }
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(json_maps);
    response.content_length(response.body().size());
    return response;
}

MapByIdApiHandler::MapByIdApiHandler(app::Application& app, const extra_data::Payload& payload)
    : app_{app}
    , payload_{payload} {
}

StringResponse MapByIdApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    auto map_id = request.target().substr(13);
    if (auto error = CheckGetOrHeadMethod(version, keep_alive, method); error) {
        return *error;
    }
    return GetMapById(std::string(map_id), version, keep_alive);
}

json::array MapByIdApiHandler::GetJsonRoads(const model::Map::Roads& roads) const {
    json::array json_roads;
    for (const auto& road : roads) {
        json::object json_road;
        if (road.IsHorizontal()) {
            json_road["x0"] = json::value_from(road.GetStart().x);
            json_road["y0"] = json::value_from(road.GetStart().y);
            json_road["x1"] = json::value_from(road.GetEnd().x);
        } else if (road.IsVertical()) {
            json_road["x0"] = json::value_from(road.GetStart().x);
            json_road["y0"] = json::value_from(road.GetStart().y);
            json_road["y1"] = json::value_from(road.GetEnd().y);
        }
        json_roads.push_back(std::move(json_road));
    }
    return json_roads;
}

json::array MapByIdApiHandler::GetJsonBuildings(const model::Map::Buildings& buildings) const {
    json::array json_buildings;
    for (const auto& building : buildings) {
        json::object json_building;
        json_building["x"] = json::value_from(building.GetBounds().position.x);
        json_building["y"] = json::value_from(building.GetBounds().position.y);
        json_building["w"] = json::value_from(building.GetBounds().size.width);
        json_building["h"] = json::value_from(building.GetBounds().size.height);
        json_buildings.push_back(std::move(json_building));
    }
    return json_buildings;
}

json::array MapByIdApiHandler::GetJsonOffices(const model::Map::Offices& offices) const {
    json::array json_offices;
    for (const auto& office : offices) {
        json::object json_office;
        json_office["id"] = json::value_from(*office.GetId());
        json_office["x"] = json::value_from(office.GetPosition().x);
        json_office["y"] = json::value_from(office.GetPosition().y);
        json_office["offsetX"] = json::value_from(office.GetOffset().dx);
        json_office["offsetY"] = json::value_from(office.GetOffset().dy);
        json_offices.push_back(std::move(json_office));
    }
    return json_offices;
}

StringResponse MapByIdApiHandler::GetMapById(const std::string& map_id, unsigned version, bool keep_alive) const {
    const model::Map* map = app_.GetGame()->FindMap(model::Map::Id(map_id));
    if (!map) {
        return MakeNotFoundError(version, keep_alive, "mapNotFound", "Map not found");
    }
    boost::json::object json_map;
    json_map["id"] = boost::json::value_from(*map->GetId());
    json_map["name"] = map->GetName();
    json_map["roads"] = std::move(GetJsonRoads(map->GetRoads()));
    json_map["buildings"] = std::move(GetJsonBuildings(map->GetBuildings()));
    json_map["offices"] = std::move(GetJsonOffices(map->GetOffices()));
    if (auto it = payload_.map_id_loot_types.find(model::Map::Id(map_id)); it != payload_.map_id_loot_types.end()) {
        json_map["lootTypes"] = it->second;
    }
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(json_map);
    response.content_length(response.body().size());
    return response;
}

JoinGameApiHandler::JoinGameApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse JoinGameApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (IsPostMethod(method)) {
        json::object request_body;
        try {
            request_body = boost::json::parse(request.body()).as_object();
        } catch (const std::exception&) {
            return MakeBadRequestError(version, keep_alive, "invalidArgument", "Join game request parse error");
        }
        return JoinGame(version, keep_alive, request_body);
    }
    return MakeMethodNotAllowedError(version, keep_alive, "POST", "invalidMethod", "Only POST method is expected");
}

StringResponse JoinGameApiHandler::JoinGame(unsigned version, bool keep_alive, const json::object& request_body) const {
    if (request_body.contains("userName") && request_body.contains("mapId")) {
        std::string name{std::move(request_body.at("userName").as_string())};
        model::Map::Id id(std::move(std::string{request_body.at("mapId").as_string()}));

        boost::json::object json_response;
        try
        {
            const auto& res = app_.JoinGame(name, id);
            json_response["authToken"] = res.player_token;
            json_response["playerId"] = res.player_id;
        } catch(const app::ApplicationError& e) {
            if (e.GetCode() == "invalidArgument") {
                return MakeBadRequestError(version, keep_alive, e.GetCode(), e.GetMessage());
            } else {
                return MakeNotFoundError(version, keep_alive, e.GetCode(), e.GetMessage());
            }
        }

        StringResponse response;
        response.version(version);
        response.keep_alive(keep_alive);
        response.result(http::status::ok);
        response.set(http::field::content_type, "application/json"sv);
        response.set(http::field::cache_control, "no-cache");
        response.body() = boost::json::serialize(json_response);
        response.content_length(response.body().size());
        return response;
    }
    return MakeBadRequestError(version, keep_alive, "invalidArgument", "Invalid request body");
}

PlayersApiHandler::PlayersApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse PlayersApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (auto error = CheckGetOrHeadMethod(version, keep_alive, method); error) {
        return *error;
    }
    std::string credentials;
    try {
        credentials = request.at(http::field::authorization);
    } catch (const std::exception&) {
        return MakeUnauthorizedError(version, keep_alive, "invalidToken", "Authorization header is missing");
    }
    return GetPlayerList(version, keep_alive, credentials);
}

StringResponse PlayersApiHandler::GetPlayerList(unsigned version, bool keep_alive, const std::string& credentials) const {
    boost::property_tree::ptree pt;
    try {
        const auto& player_list = app_.GetPlayerList(credentials);
        for (const auto& entry : player_list) {
            pt.put(std::to_string(entry.first) + ".name", entry.second);
        }
    } catch(const app::ApplicationError& e) {
        if (e.GetCode() == "invalidArgument") {
            return MakeBadRequestError(version, keep_alive, e.GetCode(), e.GetMessage());
        } else {
            return MakeUnauthorizedError(version, keep_alive, e.GetCode(), e.GetMessage());
        }
    }
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);

    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    response.body() = oss.str();
    response.content_length(response.body().size());
    return response;
}

GameStateApiHandler::GameStateApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse GameStateApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (auto error = CheckGetOrHeadMethod(version, keep_alive, method); error) {
        return *error;
    }
    std::string credentials;
    try {
        credentials = request.at(http::field::authorization);
    } catch (const std::exception&) {
        return MakeUnauthorizedError(version, keep_alive, "invalidToken", "Authorization header is missing");
    }
    return GetGameState(version, keep_alive, credentials);
}

json::object GameStateApiHandler::GetJsonGameState(const app::GameState& info) const {
    std::string dir;
    switch (info.current_dog_ptr->GetDirection())
    {
        case model::Dog::Direction::NORTH:
            dir = "U";
            break;
        case model::Dog::Direction::SOUTH:
            dir = "D";
            break;
        case model::Dog::Direction::WEST:
            dir = "L";
            break;
        case model::Dog::Direction::EAST:
            dir = "R";
            break;
    }

    json::object json_player;
    const auto& pos = info.current_dog_ptr->GetPosition();
    json_player["pos"] = json::array{pos.x, pos.y};
    const auto& speed = info.current_dog_ptr->GetSpeed();
    json_player["speed"] = json::array{speed.x, speed.y};
    json_player["dir"] = dir;

    json::array json_bag;
    json::object json_items;
    for (const auto& found_object : info.current_dog_ptr->GetBagContent()) {
        json_items["id"] = (*found_object.id);
        json_items["type"] = found_object.type;
        json_bag.emplace_back(json_items);
    }
    json_player["bag"] = json_bag;
    json_player["score"] = info.current_dog_ptr->GetScore();

    return json_player;
}

StringResponse GameStateApiHandler::GetGameState(unsigned version, bool keep_alive, const std::string& credentials) const {
    json::object json_response;
    json::object json_players;
    json::object lost_objects;
    try
    {
        for (const auto& [player_id, info] : app_.GetGameStateList(credentials)) {
            json_players[std::to_string(player_id)] = GetJsonGameState(info);
        }
        json_response["players"] = std::move(json_players);
        for (const auto& obj_ptr : app_.GetLostObjects(credentials)) {
            json::object lost_object;
            lost_object["type"] = obj_ptr->GetType();
            const auto& pos = obj_ptr->GetPosition();
            lost_object["pos"] = json::array{pos.x, pos.y};
            lost_objects[std::to_string(*(obj_ptr->GetId()))] = std::move(lost_object);
        }
        json_response["lostObjects"] = std::move(lost_objects);
    } catch(const app::ApplicationError& e) {
        return MakeUnauthorizedError(version, keep_alive, e.GetCode(), e.GetMessage());
    }

    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(json_response);
    response.content_length(response.body().size());
    return response;
}

PlayerActionApiHandler::PlayerActionApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse PlayerActionApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (auto error = CheckPostMethod(version, keep_alive, method); error) {
        return *error;
    }
    std::string credentials;
    try {
        credentials = request.at(http::field::authorization);
    } catch (const std::exception&) {
        return MakeUnauthorizedError(version, keep_alive, "invalidToken", "Authorization header is required");
    }
    json::object request_body;
    try {
        request_body = boost::json::parse(request.body()).as_object();
    } catch (const std::exception&) {
        return MakeBadRequestError(version, keep_alive, "invalidArgument", "Invalid content type");
    }
    return SetPlayerAction(version, keep_alive, credentials, request_body);
}

StringResponse PlayerActionApiHandler::SetPlayerAction(unsigned version,
                                                       bool keep_alive,
                                                       const std::string& credentials,
                                                       const json::object& request_body) const {
    if (request_body.contains("move")) {
        std::string dir{std::move(request_body.at("move").as_string())};
        try {
            app_.SetPlayerAction(credentials, dir);
        } catch(const app::ApplicationError& e) {
            if (e.GetCode() == "invalidArgument") {
                return MakeBadRequestError(version, keep_alive, e.GetCode(), e.GetMessage());
            } else {
                return MakeNotFoundError(version, keep_alive, e.GetCode(), e.GetMessage());
            }
        }

        StringResponse response;
        response.version(version);
        response.keep_alive(keep_alive);
        response.result(http::status::ok);
        response.set(http::field::content_type, "application/json"sv);
        response.set(http::field::cache_control, "no-cache");
        json::object json_response;
        response.body() = boost::json::serialize(json_response);
        response.content_length(response.body().size());
        return response;
    }
    return MakeBadRequestError(version, keep_alive, "invalidArgument", "Invalid content type");
}

TickApiHandler::TickApiHandler(app::Application& app, bool is_state_file_set, bool is_save_state_period_set, bool is_tick_period_set)
    : app_{app}
    , is_state_file_set_{is_state_file_set_}
    , is_save_state_period_set_{is_save_state_period_set}
    , is_tick_period_set_{is_tick_period_set} {
}

StringResponse TickApiHandler::Handle(const StringRequest& request) const {
    auto version = request.version();
    auto keep_alive = request.keep_alive();
    auto method = request.method();
    if (!is_tick_period_set_) {
        if (auto error = CheckPostMethod(version, keep_alive, method); error) {
            return *error;
        }
        json::object request_body;
        try {
            request_body = boost::json::parse(request.body()).as_object();
        } catch (const std::exception&) {
            return MakeBadRequestError(version, keep_alive, "invalidArgument", "Failed to parse tick request JSON");
        }
        int delta;
        try {
            delta = request_body.at("timeDelta").as_int64();
        } catch (const std::exception&) {
            return MakeBadRequestError(version, keep_alive, "invalidArgument", "Failed to parse tick request JSON");
        }
        return UpdateGameState(version, keep_alive, delta);
    }
    return MakeBadRequestError(version, keep_alive, "badRequest", "Invalid endpoint");
}

StringResponse TickApiHandler::UpdateGameState(unsigned version, bool keep_alive, int delta) const {
    app_.UpdateGameState(delta);
    if (!is_state_file_set_ && is_save_state_period_set_) {
        app_.Tick(milliseconds(delta));
    }
    StringResponse response;
    response.version(version);
    response.keep_alive(keep_alive);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json"sv);
    response.set(http::field::cache_control, "no-cache");
    json::object json_response;
    response.body() = boost::json::serialize(json_response);
    response.content_length(response.body().size());
    return response;
}

std::shared_ptr<ApiHandler> MapsApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<MapsApiHandler>(params.ref_app);
}

std::shared_ptr<ApiHandler> MapByIdApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<MapByIdApiHandler>(params.ref_app, params.payload);
}

std::shared_ptr<ApiHandler> JoinGameApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<JoinGameApiHandler>(params.ref_app);
}

std::shared_ptr<ApiHandler> PlayersApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<PlayersApiHandler>(params.ref_app);
}

std::shared_ptr<ApiHandler> GameStateApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<GameStateApiHandler>(params.ref_app);
}

std::shared_ptr<ApiHandler> PlayerActionApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<PlayerActionApiHandler>(params.ref_app);
}

std::shared_ptr<ApiHandler> TickApiHandlerFactory::CreateApiHandler(ApiHandlerParams& params) const {
    return std::make_shared<TickApiHandler>(params.ref_app, params.is_state_file_set, params.is_save_state_period_set, params.is_tick_period_set);
}

ApiHandlerManager::ApiHandlerManager(ApiHandlerParams& params)
    : params_{params} {
        endpoint_to_factory_["/api/v1/maps"] = std::make_shared<MapsApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/maps/"] = std::make_shared<MapByIdApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/game/join"] = std::make_shared<JoinGameApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/game/players"] = std::make_shared<PlayersApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/game/state"] = std::make_shared<GameStateApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/game/player/action"] = std::make_shared<PlayerActionApiHandlerFactory>();
        endpoint_to_factory_["/api/v1/game/tick"] = std::make_shared<TickApiHandlerFactory>();
}

StringResponse ApiHandlerManager::HandleApiRequest(const StringRequest& request) {
    std::string target{request.target()};
    // We check for the presence of a handler along the full path
    if (auto it = endpoint_to_factory_.find(target); it != endpoint_to_factory_.end()) {
        auto handler = it->second->CreateApiHandler(params_);
        return handler->Handle(request);
    }
    // Checking for the presence of a handler by prefix
    static const std::string prefix{"/api/v1/maps/"};
    if (target.starts_with(prefix)) {
        auto handler = endpoint_to_factory_.at(prefix)->CreateApiHandler(params_);
        return handler->Handle(request);
    }
    // If no handler is found, we return an error
    return MakeNotFoundError(request.version(), request.keep_alive(), "404 Not Found", "The entry point was not found");
}

void ApiHandlerManager::Tick(int delta) {
    params_.ref_app.UpdateGameState(delta);
    if (!params_.is_state_file_set && params_.is_save_state_period_set) {
        params_.ref_app.Tick(milliseconds(delta));
    }
}

}  // namespace api_handler
