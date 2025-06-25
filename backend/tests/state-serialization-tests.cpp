#include <sstream>
#include <filesystem>
#include <memory>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/loader/json_loader.h"
#include "../src/util/extra_data.h"
#include "../src/model/model.h"
#include "../src/app/app.h"
#include "../src/serialization/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{Dog::Id{42}, "Pluto"s, 3};
            dog.SetPosition({42.2, 12.5});
            dog.AddScore(42);
            CHECK(dog.PutToBag({FoundObject::Id{10}, 2u}));
            dog.SetDirection(Dog::Direction::EAST);
            dog.SetSpeed({2.3, -1.2});
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();
                // We check that the data after deserialization is the same as the original
                CHECK(dog.GetId() == restored.GetId());
                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetPosition() == restored.GetPosition());
                CHECK(dog.GetSpeed() == restored.GetSpeed());
                CHECK(dog.GetBagCapacity() == restored.GetBagCapacity());
                CHECK(dog.GetBagContent() == restored.GetBagContent());
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "GameSessionRepr Serialization") {
    GIVEN("A game session with dogs") {
        // Creating a Game object
        extra_data::Payload payload;
        Game game = json_loader::LoadGame("./data/config.json", payload);
        // Filling in the required objects for testing
        Map::Id id{"map1"};
        const Map* map_ptr = game.FindMap(id);
        game.AddGameSession(map_ptr);
        auto session_ptr = game.FindGameSession(id);
        session_ptr->AddDog("Rex"s, {1.0, 0.0}, 1);
        session_ptr->AddDog("Buddy"s, {2.0, 0.0}, 1);

        WHEN("the game session is serialized") {
            serialization::GameSessionRepr repr{session_ptr};
            output_archive << repr;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::GameSessionRepr restored_repr;
                input_archive >> restored_repr;
                auto restored_session = std::make_shared<model::GameSession>(*map_ptr);
                restored_repr.Restore(restored_session);
                // We check that the data after deserialization is the same as the original
                CHECK(session_ptr->GetDogs().size() == restored_session->GetDogs().size());
                CHECK(session_ptr->GetDogs()[0]->GetId() == restored_session->GetDogs()[0]->GetId());
                CHECK(session_ptr->GetDogs()[1]->GetId() == restored_session->GetDogs()[1]->GetId());
                CHECK(session_ptr->GetDogs()[0]->GetName() == restored_session->GetDogs()[0]->GetName());
                CHECK(session_ptr->GetDogs()[1]->GetName() == restored_session->GetDogs()[1]->GetName());
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "GameRepr Serialization") {
    GIVEN("A game with sessions") {
        // Creating a Game object
        extra_data::Payload payload;
        Game game = json_loader::LoadGame("./data/config.json", payload);
        // Filling in the required objects for testing
        Map::Id id{"map1"};
        const Map* map_ptr = game.FindMap(id);
        game.AddGameSession(map_ptr);
        auto session_ptr = game.FindGameSession(id);
        session_ptr->AddDog("Rex"s, {0.0, 0.1}, 0);
        session_ptr->AddDog("Buddy"s, {40.0, 0.2}, 1);
        Loot::LostObjects objects;
        objects.emplace_back(std::make_shared<LostObject>(LostObject::Id{0}, 0, geom::Point2D{0.0, 20.0}));
        objects.emplace_back(std::make_shared<LostObject>(LostObject::Id{1}, 0, geom::Point2D{30.0, 0.03}));
        session_ptr->GetMap()->GetLoot()->SetLostObjects(objects);
        session_ptr->GetMap()->GetLoot()->SetNextId(2);
        session_ptr->GetMap()->GetLoot()->SetLootCount(2);
        std::uint32_t next_dog_id = *(session_ptr->GetDogs().back()->GetId());
        ++next_dog_id;
        std::uint32_t next_object_id = *(session_ptr->GetMap()->GetLoot()->GetLostObjects().back()->GetId());
        ++next_object_id;

        WHEN("the game is serialized") {
            serialization::GameRepr repr{std::make_unique<model::Game>(game)};
            output_archive << repr;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::GameRepr restored_repr;
                input_archive >> restored_repr;
                extra_data::Payload payload2;
                Game restored_game = json_loader::LoadGame("./data/config.json", payload2);
                auto restored_game_ptr = std::make_unique<model::Game>(restored_game);
                restored_repr.Restore(restored_game_ptr);
                // We check that the data after deserialization is the same as the original
                CHECK(game.GetMaps().size() == restored_game_ptr->GetMaps().size());
                CHECK(game.GetGameSessions().size() == restored_game_ptr->GetGameSessions().size());
                CHECK(game.GetGameSessions()[0]->GetDogs().size() == restored_game_ptr->GetGameSessions()[0]->GetDogs().size());
                CHECK(game.GetGameSessions()[0]->GetDogs()[0]->GetId() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[0]->GetId());
                CHECK(game.GetGameSessions()[0]->GetDogs()[0]->GetName() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[0]->GetName());
                CHECK(game.GetGameSessions()[0]->GetDogs()[0]->GetPosition() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[0]->GetPosition());
                CHECK(game.GetGameSessions()[0]->GetDogs()[1]->GetId() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[1]->GetId());
                CHECK(game.GetGameSessions()[0]->GetDogs()[1]->GetName() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[1]->GetName());
                CHECK(game.GetGameSessions()[0]->GetDogs()[1]->GetPosition() == restored_game_ptr->GetGameSessions()[0]->GetDogs()[1]->GetPosition());
                CHECK(next_dog_id == (*(restored_game_ptr->GetGameSessions()[0]->GetDogs().back()->GetId()) + 1));
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects().size() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects().size());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetId() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetId());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetPosition() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetPosition());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetType() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[0]->GetType());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetId() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetId());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetPosition() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetPosition());
                CHECK(game.GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetType() == restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects()[1]->GetType());
                CHECK(next_object_id == (*(restored_game_ptr->GetGameSessions()[0]->GetMap()->GetLoot()->GetLostObjects().back()->GetId()) + 1));
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "ApplicationRepr Serialization") {
    GIVEN("A game, players, and player tokens") {
        // Creating a Game object
        extra_data::Payload payload;
        Game game = json_loader::LoadGame("./data/config.json", payload);
        // Filling in the required objects for testing
        Map::Id id{"map1"};
        app::Application app_real(std::make_unique<Game>(game), true);

        app_real.JoinGame("Rex"s, id);
        app_real.JoinGame("Buddy"s, id);

        app::Token token1{""};
        app::Token token2{""};

        const auto& player_tokens = app_real.GetPlayerTokens()->GetTokenToPlayer();
        for (auto it = player_tokens.begin(); it != player_tokens.end(); ++it) {
            if ("Rex"s == it->second->GetDog()->GetName()) {
                token1 = it->first;
            } else if ("Buddy"s == it->second->GetDog()->GetName()) {
                token2 = it->first;
            }
        }

        const std::string credentials1{"Bearer " + *token1};
        const std::string dir1{"D"};
        app_real.SetPlayerAction(credentials1, dir1);

        const std::string credentials2{"Bearer " + *token2};
        const std::string dir2{"R"};
        app_real.SetPlayerAction(credentials2, dir2);

        model::GameSession::Items items = {
            collision_detector::Item{geom::Point2D{0.0, 20.0}, 0},
            collision_detector::Item{geom::Point2D{30.0, 0.03}, 0}
        };
        auto game_session_ptr = app_real.GetGame()->FindGameSession(id);
        game_session_ptr->SetItems(items);

        const auto& items_real = game_session_ptr->GetItems();

        WHEN("ApplicationRepr is serialized") {
            serialization::ApplicationRepr app_repr{app_real};
            output_archive << app_repr;

            THEN("it can be deserialized") {
                extra_data::Payload payload2;
                Game restored_game = json_loader::LoadGame("./data/config.json", payload2);
                app::Application restore_app(std::make_unique<Game>(restored_game), false);
                InputArchive input_archive{strm};
                serialization::ApplicationRepr restored_app_repr;
                input_archive >> restored_app_repr;
                restored_app_repr.Restore(restore_app);
                // We check that the data after deserialization is the same as the original
                CHECK(app_real.GetGame()->FindGameSession(id)->GetDogs().size() == restore_app.GetGame()->FindGameSession(id)->GetDogs().size());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetId() == restore_app.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetId());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetId() == restore_app.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetId());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetName() == restore_app.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetName());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetName() == restore_app.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetName());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetPosition() == restore_app.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetPosition());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetPosition() == restore_app.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetPosition());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetPreviousPosition() == restore_app.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetPreviousPosition());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetPreviousPosition() == restore_app.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetPreviousPosition());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetDirection() == restore_app.GetPlayerTokens()->FindPlayerByToken(token1)->GetDog()->GetDirection());
                CHECK(app_real.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetDirection() == restore_app.GetPlayerTokens()->FindPlayerByToken(token2)->GetDog()->GetDirection());
                // We check the specific data of the players and tokens
                const auto& restored_token_to_player = restore_app.GetPlayerTokens()->GetTokenToPlayer();
                for (const auto& [token, player] : app_real.GetPlayerTokens()->GetTokenToPlayer()) {
                    if (auto it = restored_token_to_player.find(token); it != restored_token_to_player.end()) {
                        CHECK(token == it->first);
                        CHECK(player->GetDog()->GetId() == it->second->GetDog()->GetId());
                        CHECK(player->GetDog()->GetName() == it->second->GetDog()->GetName());
                    }
                }
                // We check the specific data of the items
                const auto& restored_items = restore_app.GetGame()->FindGameSession(id)->GetItems();
                CHECK(items_real[0].position == restored_items[0].position);
                CHECK(items_real[0].width == restored_items[0].width);
                CHECK(items_real[1].position == restored_items[1].position);
                CHECK(items_real[1].width == restored_items[1].width);
            }
        }
    }
}
