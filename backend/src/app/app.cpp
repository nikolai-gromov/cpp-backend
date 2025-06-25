#include "app.h"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <utility>

namespace app {

void Player::Add(const GameSessionPtr& session, const DogPtr& dog) {
    session_ = session;
    dog_ = dog;
}

const DogPtr& Player::GetDog() const {
    return dog_;
}

const GameSessionPtr& Player::GetGameSession() const {
    return session_;
}

Token PlayerTokens::Add(Player& player) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << generator1_();
    oss << std::hex << std::setw(16) << std::setfill('0') << generator2_();
    Token token{oss.str()};
    token_to_player_.emplace(token, std::make_shared<Player>(player));
    return token;
}

PlayerPtr PlayerTokens::FindPlayerByToken(const Token& token) {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

const PlayerTokens::TokenToPlayer& PlayerTokens::GetTokenToPlayer() const {
    return token_to_player_;
}

void PlayerTokens::SetTokenToPlayer(const Token& token, const PlayerPtr& player) {
    token_to_player_.emplace(token, player);
}

Player& Players::Add(const GameSessionPtr& session, const DogPtr& dog) {
    Player player_tmp;
    player_tmp.Add(session, dog);
    players_.emplace_back(std::move(player_tmp));
    auto& player = players_.back();
    map_id_to_player_list_[(player.GetGameSession()->GetMap()->GetId())][*(player.GetDog()->GetId())] = player.GetDog()->GetName();
    return player;
}

std::optional<Players::CRefPlayerList> Players::FindPlayerList(const model::Map::Id& id) const {
    if (auto it = map_id_to_player_list_.find(id); it != map_id_to_player_list_.end()) {
        return std::cref(it->second);
    }
    return std::nullopt;
}

const Players::MapIdToPlayerList& Players::GetMapIdToPlayerList() const {
    return map_id_to_player_list_;
}

const Players::AddedPlayers& Players::GetAddedPlayers() const {
    return players_;
}

ApplicationError::ApplicationError(const std::string& code, const std::string& message)
    : code_{code}
    , message_{message} {
}

const char* ApplicationError::what() const noexcept {
    return "Join Game Error";
}

const std::string& ApplicationError::GetCode() const {
    return code_;
}

const std::string& ApplicationError::GetMessage() const {
    return message_;
}

Application::Application(GamePtr game, bool random_positions)
    : game_{std::move(game)}
    , players_{std::make_unique<Players>()}
    , player_tokens_{std::make_unique<PlayerTokens>()}
    , random_positions_{random_positions} {
}

[[nodiscard]] sig::connection Application::DoOnTick(const TickSignal::slot_type& handler) {
    return tick_signal_.connect(handler);
}

void Application::Tick(milliseconds delta) {
    tick_signal_(delta);
}

const Application::GamePtr& Application::GetGame() const {
    return game_;
}

const Application::PlayersPtr& Application::GetPlayers() const {
    return players_;
}

const Application::PlayerTokensPtr& Application::GetPlayerTokens() const {
    return player_tokens_;
}

const Players::PlayerList& Application::GetPlayerList(const std::string& credentials) {
    const auto& map_id = PlayerAuthorization(credentials)->GetGameSession()->GetMap()->GetId();
    auto player_list = players_->FindPlayerList(map_id);
    if (!player_list) {
        throw ApplicationError{"invalidArgument", "The player list was not found"};
    }
    return *player_list;
}

const model::GameSession::GameStateList& Application::GetGameStateList(const std::string& credentials) {
    return PlayerAuthorization(credentials)->GetGameSession()->GetGameStateList();
}

const model::Loot::LostObjects& Application::GetLostObjects(const std::string& credentials) {
    return PlayerAuthorization(credentials)->GetGameSession()->GetMap()->GetLoot()->GetLostObjects();
}

void Application::SetPlayerAction(const std::string& credentials, const std::string& dir) {
    auto player = PlayerAuthorization(credentials);
    const auto& dog = player->GetDog();
    const double dog_speed = player->GetGameSession()->GetMap()->GetDogSpeed();
    if (dir.empty()) {
        dog->SetSpeed({0., 0.});
    } else if (dir == "L") {
        dog->SetDirection(model::Dog::Direction::WEST);
        dog->SetSpeed({-dog_speed, 0.});
    } else if (dir == "R") {
        dog->SetDirection(model::Dog::Direction::EAST);
        dog->SetSpeed({dog_speed, 0.});
    } else if (dir == "U") {
        dog->SetDirection(model::Dog::Direction::NORTH);
        dog->SetSpeed({0., -dog_speed});
    } else if (dir == "D") {
        dog->SetDirection(model::Dog::Direction::SOUTH);
        dog->SetSpeed({0., dog_speed});
    } else {
        throw ApplicationError{"invalidArgument", "Failed to parse action"};
    }
}

void Application::UpdateGameState(int delta) {
    for (const auto& map : game_->GetMaps()) {
        const auto& map_id = map.GetId();
        if (const auto& session = game_->FindGameSession(map_id); session) {
            session->UpdateGameState(delta);
        }
    }
}

const JoinGameResult& Application::JoinGame(const std::string& name, const model::Map::Id& id) {
    if (name.empty()) {
        throw ApplicationError{"invalidArgument", "Invalid name"};
    } else if (auto session = game_->FindGameSession(id); session) {
        MakeJoinGameResult(name, session);
        return result_;
    } else if (const model::Map* map = game_->FindMap(id); map) {
        game_->AddGameSession(map);
        if (auto session = game_->FindGameSession(id); session) {
            MakeJoinGameResult(name, session);
            return result_;
        } else {
            throw ApplicationError{"mapNotFound", "Map not found"};
        }
    } else {
        throw ApplicationError{"mapNotFound", "Map not found"};
    }
}

void Application::AddPlayerAndMakeResult(const std::string& user_name,
                                         const GameSessionPtr& session,
                                         const geom::Point2D& start_pos,
                                         std::size_t index) {
    auto& player = players_->Add(session, session->AddDog(user_name, start_pos, index));
    auto token_tmp = player_tokens_->Add(player);
    result_.player_token = std::move(*token_tmp);
    result_.player_id = *player.GetDog()->GetId();
}

void Application::MakeJoinGameResult(const std::string& user_name, const GameSessionPtr& session) {
    const auto& roads = session->GetMap()->GetRoads();
    if (random_positions_) {
        std::size_t index = model::GetRandomIndex(roads.size());
        AddPlayerAndMakeResult(user_name, session, model::GetRandomPosition(roads.at(index)), index);
    } else {
        AddPlayerAndMakeResult(user_name, session, {static_cast<double>(roads.at(0).GetStart().x), static_cast<double>(roads.at(0).GetStart().y)}, 0);
    }
}

PlayerPtr Application::PlayerAuthorization(const std::string& credentials) {
    if (credentials.empty() || !(credentials.starts_with("Bearer ")) || credentials.size() != 39) {
        throw ApplicationError{"invalidToken", "Authorization header is missing"};
    }
    Token token(std::move(credentials.substr(7)));
    auto player = player_tokens_->FindPlayerByToken(token);
    if (!player) {
        throw ApplicationError{"unknownToken", "Player token has not been found"};
    }
    return player;
}

}  // namespace app
