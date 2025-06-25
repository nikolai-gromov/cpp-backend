#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <functional>
#include <map>
#include <vector>


#include <boost/signals2.hpp>

#include "../model/model.h"

namespace app {

namespace detail {

struct TokenTag {};

}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;
using GameSessionPtr = std::shared_ptr<model::GameSession>;
using DogPtr = std::shared_ptr<model::Dog>;
using GameState = model::GameState;

namespace sig = boost::signals2;
using milliseconds = std::chrono::milliseconds;

class Player {
public:
    void Add(const GameSessionPtr& session, const DogPtr& dog);

    const DogPtr& GetDog() const;

    const GameSessionPtr& GetGameSession() const;

private:
    GameSessionPtr session_;
    DogPtr dog_;
};

using PlayerPtr = std::shared_ptr<Player>;

class PlayerTokens {
public:
    using TokenHasher = util::TaggedHasher<Token>;
    using TokenToPlayer = std::unordered_map<Token, PlayerPtr, TokenHasher>;

    Token Add(Player& player);

    PlayerPtr FindPlayerByToken(const Token& token);

    const TokenToPlayer& GetTokenToPlayer() const;

    void SetTokenToPlayer(const Token& token, const PlayerPtr& player);

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    TokenToPlayer token_to_player_;
};

class Players {
public:
    using AddedPlayers = std::vector<Player>;
    using PlayerList = std::map<std::uint32_t, std::string>;
    using CRefPlayerList = std::reference_wrapper<const PlayerList>;
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    using MapIdToPlayerList = std::unordered_map<model::Map::Id, PlayerList, MapIdHasher>;

    Player& Add(const GameSessionPtr& session, const DogPtr& dog);

    std::optional<CRefPlayerList> FindPlayerList(const model::Map::Id& id) const;

    const MapIdToPlayerList& GetMapIdToPlayerList() const;

    const AddedPlayers& GetAddedPlayers() const;

private:
    AddedPlayers players_;
    MapIdToPlayerList map_id_to_player_list_;
};

class ApplicationError : public std::exception {
public:
    ApplicationError(const std::string& code, const std::string& message);

    const char* what() const noexcept override;

    const std::string& GetCode() const;

    const std::string& GetMessage() const;

private:
    std::string code_;
    std::string message_;
};

struct JoinGameResult {
    std::string player_token;
    std::uint32_t player_id;
};

class Application {
public:
    using TickSignal = sig::signal<void(milliseconds delta)>;
    using milliseconds = std::chrono::milliseconds;
    using GamePtr = std::unique_ptr<model::Game>;
    using PlayersPtr = std::unique_ptr<Players>;
    using PlayerTokensPtr = std::unique_ptr<PlayerTokens>;

    Application(GamePtr game, bool random_positions);

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] sig::connection DoOnTick(const TickSignal::slot_type& handler);

    void Tick(milliseconds delta);

    const GamePtr& GetGame() const;
    const PlayersPtr& GetPlayers() const;
    const PlayerTokensPtr& GetPlayerTokens() const;

    const Players::PlayerList& GetPlayerList(const std::string& credentials);

    const model::GameSession::GameStateList& GetGameStateList(const std::string& credentials);

    const model::Loot::LostObjects& GetLostObjects(const std::string& credentials);

    void SetPlayerAction(const std::string& credentials, const std::string& dir);

    void UpdateGameState(int delta);

    const JoinGameResult& JoinGame(const std::string& name, const model::Map::Id& id);


private:
    GamePtr game_;
    PlayersPtr players_;
    PlayerTokensPtr player_tokens_;
    bool random_positions_;
    TickSignal tick_signal_;
    JoinGameResult result_;

    void AddPlayerAndMakeResult(const std::string& user_name,
                                    const GameSessionPtr& session,
                                    const geom::Point2D& start_pos,
                                    std::size_t index);

    void MakeJoinGameResult(const std::string& user_name, const GameSessionPtr& session);

    PlayerPtr PlayerAuthorization(const std::string& credentials);

};

}  // namespace app
