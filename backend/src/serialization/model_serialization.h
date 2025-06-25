#include <memory>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include "../app/app.h"
#include "../model/model.h"
#include "../detector/collision_detector.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar&(*obj.id);
    ar&(obj.type);
}

}  // namespace model

namespace collision_detector {

template <typename Archive>
void serialize(Archive& ar, Item& item, [[maybe_unused]] const unsigned version) {
    ar& item.position;
    ar& item.width;
}

}  // namespace collision_detector

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , bag_cap_(dog.GetBagCapacity())
        , current_pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , dir_(dog.GetDirection())
        , current_index_(dog.GetCurrentRoadsIndex())
        , previous_pos_(dog.GetPreviousPosition())
        , bag_(dog.GetBagContent())
        , score_(dog.GetScore()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{id_, name_, bag_cap_};
        dog.SetPosition(current_pos_);
        dog.SetSpeed(speed_);
        dog.SetDirection(dir_);
        dog.SetCurrentRoadsIndex(current_index_);
        dog.AddScore(score_);
        for (const auto& item : bag_) {
            if (!dog.PutToBag(item)) {
                throw std::runtime_error("Failed to put bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& *id_;
        ar& name_;
        ar& bag_cap_;
        ar& current_pos_;
        ar& speed_;
        ar& dir_;
        ar& current_index_;
        ar& previous_pos_;
        ar& bag_;
        ar& score_;
    }

private:
    model::Dog::Id id_ = model::Dog::Id{0u};
    std::string name_;
    std::size_t bag_cap_ = 0;
    geom::Point2D current_pos_;
    geom::Vec2D speed_;
    model::Dog::Direction dir_ = model::Dog::Direction::NORTH;
    std::size_t current_index_;
    geom::Point2D previous_pos_;
    model::Dog::BagContent bag_;
    model::Dog::Score score_ = 0;
};

class LostObjectRepr {
public:
    LostObjectRepr() = default;

    LostObjectRepr(const model::LostObject& object)
        : id_(object.GetId())
        , type_(object.GetType())
        , pos_(object.GetPosition()) {
    }

    [[nodiscard]] model::LostObject Restore() const {
        model::LostObject object{id_, type_, pos_};
        return object;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& *id_;
        ar& type_;
        ar& pos_;
    }

private:
    model::LostObject::Id id_ =  model::LostObject::Id{0u};
    unsigned int type_ = 0u;
    geom::Point2D pos_;
};

class LootRepr {
public:
    LootRepr() = default;

    explicit LootRepr(const model::Map::LootPtr& loot) {
        for (const auto& object_ptr : loot->GetLostObjects()) {
            objects_.emplace_back(LostObjectRepr(*object_ptr));
        }
    }

    void Restore(const model::Map::LootPtr& loot) const {
        model::Loot::LostObjects objects;
        objects.reserve(objects_.size());
        for (const auto& object : objects_) {
            objects.emplace_back(std::make_shared<model::LostObject>(object.Restore()));
        }
        loot->SetLostObjects(objects);
        loot->SetNextId(next_id_);
        loot->SetLootCount(loot_count_);
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& loot_count_;
        ar& next_id_;
        ar& objects_;
    }

private:
    unsigned int loot_count_ = 0u;
    std::uint32_t next_id_ = 0u;
    std::vector<LostObjectRepr> objects_;
};

class MapRepr {
public:
    MapRepr() = default;

    explicit MapRepr(const model::Map& map)
        : loot_(map.GetLoot()) {
    }

    void Restore(const model::Map* map) const {
        loot_.Restore(map->GetLoot());
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& loot_;
    }


private:
    LootRepr loot_;
};

class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(const model::Game::GameSessionPtr& session) {
        map_ = MapRepr(*(session->GetMap()));
        next_id_ = *(session->GetDogs().back()->GetId());
        ++next_id_;

        for (const auto& dog_ptr : session->GetDogs()) {
            dogs_.emplace_back(DogRepr(*dog_ptr));
        }

        for (const auto& item : session->GetItems()) {
            items_.emplace_back(item);
        }
    }

    void Restore(const model::Game::GameSessionPtr& session) const {
        map_.Restore(session->GetMap());
        session->SetNextId(next_id_);

        model::GameSession::Dogs dogs;
        dogs.reserve(dogs_.size());
        for (const auto& dog_repr : dogs_) {
            dogs.emplace_back(std::make_shared<model::Dog>(dog_repr.Restore()));
        }
        session->SetDogs(dogs);

        model::GameSession::Items items;
        items.reserve(items_.size());
        for (const auto& item : items_) {
            items.emplace_back(item);
        }
        session->SetItems(items);
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& map_;
        ar& next_id_;
        ar& dogs_;
        ar& items_;
    }

private:
    MapRepr map_;
    std::uint32_t next_id_ = 0u;
    std::vector<DogRepr> dogs_;
    std::vector<collision_detector::Item> items_;
};

class GameRepr {
public:
    GameRepr() = default;

    explicit GameRepr(const std::unique_ptr<model::Game>& game) {
        for (const auto& map : game->GetMaps()) {
            if (auto session_ptr = game->FindGameSession(map.GetId()); session_ptr) {
                id_maps_.emplace_back(*(map.GetId()));
                sessions_.emplace_back(GameSessionRepr(session_ptr));
            }
        }
    }

    void Restore(const std::unique_ptr<model::Game>& game) const {
        for (const auto& map_id : id_maps_) {
            if (const auto map_ptr = game->FindMap(model::Map::Id{map_id}); map_ptr) {
                game->AddGameSession(map_ptr);
                for (const auto& session_repr : sessions_) {
                    session_repr.Restore(game->GetGameSessions().back());
                }
            }
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& id_maps_;
        ar& sessions_;
    }


private:
    std::vector<std::string> id_maps_;
    std::vector<GameSessionRepr> sessions_;
};

class PlayersRepr {
public:
    PlayersRepr()  = default;

    explicit PlayersRepr(const std::unique_ptr<app::Players>& players) {
        for (const auto& [map_id, player_list] : players->GetMapIdToPlayerList()) {
            map_id_to_player_list_.emplace(*map_id, player_list);
        }
    }

    void Restore(const std::unique_ptr<model::Game>& game, const std::unique_ptr<app::Players>& players) const {
        for (const auto& [map_id, player_list] : map_id_to_player_list_) {
            model::Map::Id id(map_id);
            if (auto session_ptr = game->FindGameSession(id); session_ptr) {
                const auto& dogs = session_ptr->GetDogs();
                for (const auto& dog_ptr : dogs) {
                    if (auto it = player_list.find(*(dog_ptr->GetId())); it != player_list.end()) {
                        players->Add(session_ptr, dog_ptr);
                    }
                }
            }
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& map_id_to_player_list_;
    }

private:
    std::unordered_map<std::string, std::map<std::uint32_t, std::string>> map_id_to_player_list_;
};

class PlayerTokensRepr {
public:
    PlayerTokensRepr() = default;

    explicit PlayerTokensRepr(const std::unique_ptr<app::PlayerTokens>& player_tokens) {
        for (const auto& [token, player_ptr] : player_tokens->GetTokenToPlayer()) {
            token_to_player_.emplace(*token, *(player_ptr->GetDog()->GetId()));
        }
    }

    void Restore(const std::unique_ptr<model::Game>& game,
                 const std::unique_ptr<app::Players>& players,
                 const std::unique_ptr<app::PlayerTokens>& player_tokens) {
        const auto& array_of_players = players->GetAddedPlayers();
        for (const auto& [token, player_id] : token_to_player_) {
            if (auto it = std::find_if(
                                        array_of_players.begin(),
                                        array_of_players.end(),
                                        [&player_id](const auto& player) {
                                            return player_id == (*player.GetDog()->GetId());
                                        }
                                       );
                    it != array_of_players.end()) {
                auto player = *it;
                player_tokens->SetTokenToPlayer(app::Token{token}, std::make_shared<app::Player>(player));
            }
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & token_to_player_;
    }

private:
    std::unordered_map<std::string, std::uint32_t> token_to_player_;
};

class ApplicationRepr {
public:
    ApplicationRepr() = default;

    ApplicationRepr(const app::Application& app)
        : game_{app.GetGame()}
        , players_{app.GetPlayers()}
        , player_tokens_{app.GetPlayerTokens()} {
    }

    void Restore(app::Application& app) {
        game_.Restore(app.GetGame());
        players_.Restore(app.GetGame(), app.GetPlayers());
        player_tokens_.Restore(app.GetGame(), app.GetPlayers(), app.GetPlayerTokens());
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & game_;
        ar & players_;
        ar & player_tokens_;
    }

private:
    GameRepr game_;
    PlayersRepr players_;
    PlayerTokensRepr player_tokens_;
};

}  // namespace serialization