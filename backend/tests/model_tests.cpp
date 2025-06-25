#include <catch2/catch_test_macros.hpp>

#include "../src/util/geom.h"
#include "../src/model/model.h"

using namespace std::literals;
using namespace model;

TEST_CASE("Point creation and comparison") {
    Point p1{10, 20};
    Point p2{10, 20};
    Point p3{15, 25};

    CHECK(p1.x == p2.x);
    CHECK(p1.y != p3.y);
}

TEST_CASE("Rectangle creation and bounds") {
    Rectangle rect{{5, 5}, {10, 10}};
    CHECK(rect.position.x == 5);
    CHECK(rect.position.y == 5);
    CHECK(rect.size.width == 10);
    CHECK(rect.size.height == 10);
}

TEST_CASE("Road creation and direction") {
    Point start{0, 0};
    Coord end = 10;

    Road horizontalRoad{Road::Direction::HORIZONTAL, start, end};
    CHECK(horizontalRoad.IsHorizontal());
    CHECK(!horizontalRoad.IsVertical());
    CHECK(horizontalRoad.GetStart().x == start.x);
    CHECK(horizontalRoad.GetStart().y == start.y);
    CHECK(horizontalRoad.GetEnd().x == end);
    CHECK(horizontalRoad.GetEnd().y == 0);

    Road verticalRoad{Road::Direction::VERTICAL, start, end};
    CHECK(verticalRoad.IsVertical());
    CHECK(!verticalRoad.IsHorizontal());
    CHECK(verticalRoad.GetStart().x == start.x);
    CHECK(verticalRoad.GetStart().y == start.y);
    CHECK(verticalRoad.GetEnd().x == 0);
    CHECK(verticalRoad.GetEnd().y == end);
}

TEST_CASE("Building bounds") {
    Rectangle bounds{{0, 0}, {5, 5}};
    Building building{bounds};

    CHECK(building.GetBounds().position.x == bounds.position.x);
    CHECK(building.GetBounds().position.y == bounds.position.y);
    CHECK(building.GetBounds().size.height == bounds.size.height);
    CHECK(building.GetBounds().size.width == bounds.size.width);
}

TEST_CASE("Office creation and accessors") {
    Office::Id officeId{"office1"};
    Point position{1, 2};
    Offset offset{3, 4};

    Office office{officeId, position, offset};

    CHECK(office.GetId() == officeId);
    CHECK(office.GetPosition().x == position.x);
    CHECK(office.GetPosition().y == position.y);
    CHECK(office.GetOffset().dx == offset.dx);
    CHECK(office.GetOffset().dy == offset.dy);
}

TEST_CASE("FoundObject comparison") {
    FoundObject obj1{FoundObject::Id{1}, 2};
    FoundObject obj2{FoundObject::Id{1}, 2};
    FoundObject obj3{FoundObject::Id{2}, 3};

    CHECK(obj1 == obj2);
    CHECK(obj1 != obj3);
}

TEST_CASE("LostObject creation and accessors") {
    LostObject::Id id{1};
    unsigned int type = 2;
    geom::Point2D position{3.0, 4.0};

    LostObject lostObject{id, type, position};

    CHECK(lostObject.GetId() == id);
    CHECK(lostObject.GetType() == type);
    CHECK(lostObject.GetPosition() == position);
}

TEST_CASE("Loot creation and functionality") {
    GeneratorSettings settings{1.0, 0.5}; // The period is 1 sec, the probability is 50%
    std::vector<unsigned int> values{10, 20, 30};
    Loot loot{settings, 3, values};

    CHECK(loot.GetLootTypesCount() == 3);
    CHECK(loot.GetValue(0) == 10);
    CHECK(loot.GetValue(1) == 20);
    CHECK(loot.GetValue(2) == 30);

    // Checking the addition LostObject
    auto lostObject = loot.AddLostObject(1, {5.0, 5.0});
    CHECK(loot.GetLostObjects().size() == 1);
    CHECK(loot.GetLostObjects()[0]->GetId() == lostObject->GetId());
    CHECK(loot.GetLostObjects()[0]->GetType() == 1);
    CHECK(loot.GetLostObjects()[0]->GetPosition() == geom::Point2D{5.0, 5.0});
}

TEST_CASE("Map creation and accessors") {
    Map::Id mapId{"map1"};
    Map map{mapId, "Test Map", 2.5, 10};

    CHECK(map.GetId() == mapId);
    CHECK(map.GetName() == "Test Map");
    CHECK(map.GetDogSpeed() == 2.5);
    CHECK(map.GetBagCapacity() == 10);
}

TEST_CASE("Map adding roads, buildings, and offices") {
    Map::Id mapId{"map2"};
    Map map{mapId, "Another Test Map", 3.0, 15};

    Road road{Road::Direction::HORIZONTAL, {0, 0}, 10};
    Building building{{{1, 1}, {2, 2}}};
    Office office{Office::Id{"office1"}, {2, 2}, {1, 1}};

    map.AddRoad(road);
    map.AddBuilding(building);
    map.AddOffice(office);

    CHECK(map.GetRoads().size() == 1);
    CHECK(map.GetBuildings().size() == 1);
    CHECK(map.GetOffices().size() == 1);
    CHECK(map.GetOffices()[0].GetId() == office.GetId());
}

TEST_CASE("Map adding duplicate office throws exception") {
    Map::Id mapId{"map3"};
    Map map{mapId, "Map with Duplicate Office", 4.0, 20};

    Office office{Office::Id{"office1"}, {0, 0}, {1, 1}};
    map.AddOffice(office);

    CHECK_THROWS_AS(map.AddOffice(office), std::invalid_argument);
}

TEST_CASE("Dog creation and accessors") {
    Dog::Id id{1};
    std::string name = "Buddy";
    size_t bag_capacity = 5;

    Dog dog{id, name, bag_capacity};

    CHECK(dog.GetId() == id);
    CHECK(dog.GetName() == name);
    CHECK(dog.GetBagCapacity() == bag_capacity);
    CHECK(dog.GetScore() == 0);
    CHECK(dog.IsBagFull() == false);
}

TEST_CASE("Dog position and movement") {
    Dog::Id id{2};
    Dog dog{id, "Max", 3};

    geom::Point2D initial_pos{0.0, 0.0};
    dog.SetPosition(initial_pos);
    CHECK(dog.GetPosition() == initial_pos);

    geom::Point2D new_pos{1.0, 1.0};
    dog.SetPosition(new_pos);
    CHECK(dog.GetPosition() == new_pos);
    CHECK(dog.GetPreviousPosition() == initial_pos);
}

TEST_CASE("Dog bag functionality") {
    Dog::Id id{3};
    Dog dog{id, "Rocky", 2};

    FoundObject obj1{FoundObject::Id{1}, 10};
    FoundObject obj2{FoundObject::Id{2}, 20};

    CHECK(dog.PutToBag(obj1) == true);
    CHECK(dog.PutToBag(obj2) == true);
    CHECK(dog.IsBagFull() == true);

    FoundObject obj3{FoundObject::Id{3}, 30};
    CHECK(dog.PutToBag(obj3) == false); // Bag should be full

    CHECK(dog.EmptyBag() == 2); // Check if emptying bag returns correct size
    CHECK(dog.IsBagFull() == false);
}

TEST_CASE("Dog score management") {
    Dog::Id id{4};
    Dog dog{id, "Bella", 5};

    dog.AddScore(10);
    CHECK(dog.GetScore() == 10);

    dog.AddScore(5);
    CHECK(dog.GetScore() == 15);
}

TEST_CASE("GameSession creation and dog management") {
    Map::Id mapId{"map1"};
    Map map{mapId, "First Map", 2.5, 10};
    GameSession session{map};

    CHECK(session.GetMap() == &map);
    CHECK(session.GetDogs().size() == 0);

    auto dog = session.AddDog("Charlie", {0.0, 0.0}, 0);
    CHECK(session.GetDogs().size() == 1);
    CHECK(session.GetDogs()[0]->GetName() == "Charlie");
}

TEST_CASE("GameSession setting dogs") {
    Map::Id mapId{"map2"};
    Map map{mapId, "Second Map", 3.0, 15};
    GameSession session{map};

    auto dog1 = session.AddDog("Dog1", {0.0, 0.0}, 0);
    auto dog2 = session.AddDog("Dog2", {1.0, 1.0}, 1);

    CHECK(session.GetDogs().size() == 2);

    GameSession::Dogs newDogs;
    newDogs.push_back(dog1);
    newDogs.push_back(dog2);
    session.SetDogs(newDogs);

    CHECK(session.GetDogs().size() == 2);
}

TEST_CASE("Game creation and session management") {
    Game game;

    Map::Id mapId{"map3"};
    Map map{mapId, "Third Map", 4.0, 20};
    game.AddMap(map);

    CHECK(game.GetMaps().size() == 1);
    CHECK(game.FindMap(mapId) != nullptr);

    game.AddGameSession(&map);
    CHECK(game.GetGameSessions().size() == 1);
    CHECK(game.FindGameSession(mapId) != nullptr);
}

TEST_CASE("Game adding duplicate map throws exception") {
    Game game;

    Map::Id mapId{"map4"};
    Map map{mapId, "Fourth Map", 5.0, 25};
    game.AddMap(map);

    CHECK_THROWS_AS(game.AddMap(map), std::invalid_argument);
}