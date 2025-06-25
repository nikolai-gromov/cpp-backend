#pragma once

#include <memory>
#include <random>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include "../util/geom.h"
#include "../util/tagged.h"
#include "../generator/loot_generator.h"
#include "../detector/collision_detector.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct GeneratorSettings {
    double period;
    double probability;
};

struct FoundObject {
    using Id = util::Tagged<std::uint32_t, FoundObject>;
    using LostObjectType = unsigned int;

    Id id{0u};
    LostObjectType type{0u};

    [[nodiscard]] auto operator<=>(const FoundObject&) const = default;
};

class Road {
public:
    enum class Direction {
        HORIZONTAL,
        VERTICAL
    };

    Road(Direction direction, Point start, Coord end) noexcept;

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;

    const Point& GetStart() const noexcept;
    const Point& GetEnd() const noexcept;

    const geom::Point2D& GetMin() const noexcept;
    const geom::Point2D& GetMax() const noexcept;

private:
    Direction direction_;
    Point start_;
    Point end_;
    geom::Point2D min_;
    geom::Point2D max_;
    static constexpr double offset_from_axis_{0.4};

    void SetBounds();
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept;

    const Id& GetId() const noexcept;
    const Point& GetPosition() const noexcept;
    const Offset& GetOffset() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class LostObject {
public:
    using Id = util::Tagged<std::uint32_t, LostObject>;

    LostObject(Id id, unsigned int type, const geom::Point2D& pos) noexcept;

    Id GetId() const noexcept;

    unsigned GetType() const noexcept;

    const geom::Point2D& GetPosition() const noexcept;

private:
    Id id_;
    unsigned int type_;
    geom::Point2D pos_;
};

class Loot {
public:
    using LostObjectPtr = std::shared_ptr<LostObject>;
    using LostObjects = std::vector<LostObjectPtr>;

    Loot(const GeneratorSettings& settings, unsigned int loot_types_count, const std::vector<unsigned int>& values) noexcept;

    unsigned int GetLootCount(unsigned int looter_count) const noexcept;
    unsigned int GetLooterCount() const noexcept;
    unsigned int GetLootTypesCount() const noexcept;
    unsigned int GetValue(unsigned int type) const noexcept;
    const LostObjects& GetLostObjects() const noexcept;

    LostObjectPtr AddLostObject(unsigned int type, const geom::Point2D& pos);

    void SetLostObjects(const LostObjects& objects);
    void SetNextId(std::uint32_t next_id);
    void SetLootCount(unsigned int loot_count);

    void RemoveLostObject(const LostObjectPtr& object);

private:
    GeneratorSettings settings_;
    unsigned int loot_types_count_;
    std::vector<unsigned int> values_;
    mutable unsigned int looter_count_;
    mutable unsigned int loot_count_;
    mutable std::chrono::steady_clock::time_point last_generate_time_ = std::chrono::steady_clock::now();
    std::uint32_t next_id_{0u};
    LostObjects objects_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    using LootPtr = std::shared_ptr<Loot>;

    Map(Id id, std::string name, double dog_speed, std::size_t bag_capacity) noexcept;

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    double GetDogSpeed() const noexcept;
    std::size_t GetBagCapacity() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;
    const LootPtr& GetLoot() const noexcept;

    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);
    void AddOffice(const Office& office);
    void AddLoot(const GeneratorSettings& settings, unsigned int loot_types_count, const std::vector<unsigned int>& values);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, std::size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_;
    std::size_t bag_capacity_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    LootPtr loot_;
};

bool IsWithinRoadBounds(const geom::Point2D& pos, const model::Road& road);

class Dog {
public:
    using Id = util::Tagged<std::uint32_t, Dog>;
    using BagContent = std::vector<FoundObject>;
    using Score = unsigned int;

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    Dog(Id id, const std::string& name, size_t bag_cap) noexcept;

    Id GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const geom::Point2D& GetPosition() const noexcept;
    const geom::Point2D& GetPreviousPosition() const noexcept;
    const geom::Vec2D& GetSpeed() const noexcept;
    const Direction& GetDirection() const noexcept;
    std::size_t GetCurrentRoadsIndex() const noexcept;

    void SetPosition(const geom::Point2D& pos);
    void SetSpeed(const geom::Vec2D& speed);
    void SetDirection(Direction dir);
    void SetCurrentRoadsIndex(std::size_t index);

    void SetPositionWhenMovingWest(const Map::Roads& roads, int delta);
    void SetPositionWhenMovinggEast(const Map::Roads& roads, int delta);
    void SetPositionWhenMovingNorth(const Map::Roads& roads, int delta);
    void SetPositionWhenMovingSouth(const Map::Roads& roads, int delta);

    std::size_t GetBagCapacity() const noexcept;
    const BagContent& GetBagContent() const noexcept;

    std::size_t EmptyBag() noexcept;

    bool IsBagFull() const noexcept;

    [[nodiscard]] bool PutToBag(FoundObject item);

    Score GetScore() const noexcept;

    void AddScore(Score score) noexcept;

private:
    Id id_;
    std::string name_;
    std::size_t bag_cap_;
    geom::Point2D current_pos_;
    geom::Vec2D speed_;
    Direction dir_;
    std::size_t current_index_;
    geom::Point2D previous_pos_;
    BagContent bag_;
    Score score_{};
};

struct GameState;

std::size_t GetRandomIndex(std::size_t count);

geom::Point2D GetRandomPosition(const Road& road);

unsigned int GetRandomType(unsigned int loot_types_count);

class GameSession {
public:
    using DogPtr = std::shared_ptr<Dog>;
    using Dogs = std::vector<DogPtr>;
    using GameStateList = std::map<std::uint32_t, GameState>;
    using Gatherers = std::vector<collision_detector::Gatherer>;
    using Items = std::vector<collision_detector::Item>;
    using Bases = std::vector<collision_detector::Item>;

    explicit GameSession(const Map& map) noexcept;

    DogPtr AddDog(const std::string& name, const geom::Point2D& pos, std::size_t index);

    const Map* GetMap() const noexcept;
    const Dogs& GetDogs() const noexcept;
    const GameStateList& GetGameStateList() const noexcept;
    const Items& GetItems() const noexcept;

    void SetNextId(std::uint32_t next_id);
    void SetDogs(const Dogs& dogs);
    void SetItems(const Items& items);

    void UpdateGameState(int delta);

private:
    const Map* map_;
    std::uint32_t next_id_{0u};
    Dogs dogs_;

    GameStateList game_state_list_;
    Gatherers gatherers_;
    Items items_;
    Bases bases_;

    void SetLocationDogs(const DogPtr& dog_ptr, int delta);

    void UpdateDogs(int delta);

    void UpdateLostObjects();

    void ProcessGatherEvents();

    void ProcessReturnToBaseEvents();
};

struct GameState {
    std::shared_ptr<const Dog> current_dog_ptr;

    GameState(GameSession::DogPtr dog_ptr);
};

class Game {
public:
    using Maps = std::vector<Map>;
    using GameSessionPtr = std::shared_ptr<GameSession>;
    using GameSessions = std::vector<GameSessionPtr>;

    void AddMap(Map map);

    void AddGameSession(const model::Map* map);

    const Maps& GetMaps() const noexcept;
    const GameSessions& GetGameSessions() const noexcept;

    const Map* FindMap(const Map::Id& id) const noexcept;
    GameSessionPtr FindGameSession(const Map::Id& id) const noexcept;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
    GameSessions sessions_;
};

}  // namespace model
