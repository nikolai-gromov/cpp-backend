#include "model.h"

#include <algorithm>
#include <stdexcept>
#include <chrono>

namespace model {
using namespace std::literals;

Road::Road(Direction direction, Point start, Coord end) noexcept
    : direction_{direction}
    , start_{start}
    , end_{(direction == Direction::HORIZONTAL) ? Point{end, 0} : Point{0, end}} {
    SetBounds();
}

void Road::SetBounds() {
    double min_x, max_x, min_y, max_y;
    if (direction_ == Direction::HORIZONTAL) {
        min_x = std::min(start_.x, end_.x) - offset_from_axis_;
        max_x = std::max(start_.x, end_.x) + offset_from_axis_;
        min_y = start_.y - offset_from_axis_;
        max_y = start_.y + offset_from_axis_;
    } else {
        min_x = start_.x - offset_from_axis_;
        max_x = start_.x + offset_from_axis_;
        min_y = std::min(start_.y, end_.y) - offset_from_axis_;
        max_y = std::max(start_.y, end_.y) + offset_from_axis_;
    }
    min_ = {min_x, min_y};
    max_ = {max_x, max_y};
}

bool Road::IsHorizontal() const noexcept {
    return direction_ == Direction::HORIZONTAL;
}

bool Road::IsVertical() const noexcept {
    return direction_ == Direction::VERTICAL;
}

const Point& Road::GetStart() const noexcept {
    return start_;
}

const Point& Road::GetEnd() const noexcept {
    return end_;
}

const geom::Point2D& Road::GetMin() const noexcept {
    return min_;
}

const geom::Point2D& Road::GetMax() const noexcept {
    return max_;
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{bounds} {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Office::Id id, Point position, Offset offset) noexcept
    : id_{std::move(id)}
    , position_{position}
    , offset_{offset} {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

const Point& Office::GetPosition() const noexcept {
    return position_;
}

const Offset& Office::GetOffset() const noexcept {
    return offset_;
}

LostObject::LostObject(LostObject::Id id, unsigned int type, const geom::Point2D& pos) noexcept
    : id_(std::move(id))
    , type_{type}
    , pos_{pos.x, pos.y} {
}

LostObject::Id LostObject::GetId() const noexcept {
    return id_;
}

unsigned LostObject::GetType() const noexcept {
    return type_;
}

const geom::Point2D& LostObject::GetPosition() const noexcept {
    return pos_;
}

Loot::Loot(const GeneratorSettings& settings, unsigned int loot_types_count, const std::vector<unsigned int>& values) noexcept
    : settings_{settings.period, settings.probability}
    , loot_types_count_{loot_types_count}
    , values_(values) {
}

unsigned int Loot::GetLootCount(unsigned int looter_count) const noexcept {
    // Updating the value
    looter_count_ = looter_count;
    // Сalculating the time interval that has elapsed since the previous call to Generate()
    auto now = std::chrono::steady_clock::now();
    auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_generate_time_);
    last_generate_time_ = now;
    // Calculate the amount of loot that should appear on the map after a given time interval
    const double second = 1000.;
    auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>{settings_.period * second});
    loot_gen::LootGenerator generator(interval, settings_.probability);
    loot_count_ = generator.Generate(time_delta, loot_count_, looter_count_);
    return loot_count_;
}

unsigned int Loot::GetLooterCount() const noexcept {
    return looter_count_;
}

unsigned int Loot::GetLootTypesCount() const noexcept {
    return loot_types_count_;
}

unsigned int Loot::GetValue(unsigned int type) const noexcept {
    return values_.at(type);
}

const Loot::LostObjects& Loot::GetLostObjects() const noexcept {
    return objects_;
}

Loot::LostObjectPtr Loot::AddLostObject(unsigned int type, const geom::Point2D& pos)  {
    std::uint32_t id = next_id_;
    ++next_id_;
    auto object_ptr = std::make_shared<LostObject>(LostObject::Id{id}, type, pos);
    objects_.emplace_back(object_ptr);
    return object_ptr;
}

void Loot::SetLostObjects(const Loot::LostObjects& objects) {
    objects_.reserve(objects.size());
    objects_ = std::move(objects);
}

void Loot::SetNextId(std::uint32_t next_id) {
    next_id_ = next_id;
}

void Loot::SetLootCount(unsigned int loot_count) {
    loot_count_ = loot_count;
}

void Loot::RemoveLostObject(const Loot::LostObjectPtr& object) {
    auto it = std::remove_if(objects_.begin(), objects_.end(),
        [&object](const Loot::LostObjectPtr& lost_object) {
            return lost_object->GetId() == object->GetId();
        });
    objects_.erase(it, objects_.end());
}

Map::Map(Map::Id id, std::string name, double dog_speed, std::size_t bag_capacity) noexcept
    : id_(std::move(id))
    , name_(std::move(name))
    , dog_speed_{dog_speed}
    , bag_capacity_{bag_capacity} {
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

double Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}

std::size_t Map::GetBagCapacity() const noexcept {
    return bag_capacity_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

const Map::LootPtr& Map::GetLoot() const noexcept {
    return loot_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
}

void Map::AddOffice(const Office& office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Deleting the office from the vector if it was not possible to insert it into the unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::AddLoot(const GeneratorSettings& settings, unsigned int loot_types_count, const std::vector<unsigned int>& values) {
    loot_ = std::make_shared<Loot>(settings, loot_types_count, values);
}

bool IsWithinRoadBounds(const geom::Point2D& pos, const model::Road& road) {
    return pos.x >= road.GetMin().x && pos.x <= road.GetMax().x && pos.y >= road.GetMin().y && pos.y <= road.GetMax().y;
}

Dog::Dog(Dog::Id id, const std::string& name, size_t bag_cap) noexcept
    : id_{id}
    , name_{name}
    , bag_cap_{bag_cap} {
}

Dog::Id Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

const geom::Point2D& Dog::GetPosition() const noexcept {
    return current_pos_;
}

const geom::Point2D& Dog::GetPreviousPosition() const noexcept {
    return previous_pos_;
}

const geom::Vec2D& Dog::GetSpeed() const noexcept {
    return speed_;
}

const Dog::Direction& Dog::GetDirection() const noexcept {
    return dir_;
}

std::size_t Dog::GetCurrentRoadsIndex() const noexcept {
    return current_index_;
}

void Dog::SetPosition(const geom::Point2D& pos) {
    previous_pos_ = {current_pos_.x, current_pos_.y};
    current_pos_ = {pos.x, pos.y};
}

void Dog::SetSpeed(const geom::Vec2D& speed) {
    speed_ = {speed.x, speed.y};
}

void Dog::SetDirection(Dog::Direction dir) {
    dir_ = dir;
}

void Dog::SetCurrentRoadsIndex(std::size_t index) {
    current_index_ = index;
}

void Dog::SetPositionWhenMovingWest(const Map::Roads& roads, int delta) {
    double new_x, new_y;
    const double second = 1000.;
    new_x = GetPosition().x + GetSpeed().x * (delta / second);
    new_y = GetPosition().y;
    geom::Point2D new_pos;
    new_pos = {new_x, new_y};

    const auto& current_road = roads.at(GetCurrentRoadsIndex());
    if (IsWithinRoadBounds(new_pos, current_road)) {
        SetPosition({new_pos.x, new_pos.y});
    } else {
        for (std::size_t i = 0; i != roads.size(); ++i) {
            // Checking for the transition between vertical and horizontal roads
            // We check whether the new position is in the range of the next road
            if (current_road.IsVertical() && roads[i].IsHorizontal() &&
                new_pos.y >= roads[i].GetMin().y && new_pos.y <= roads[i].GetMax().y) {
                if (current_road.GetStart().x <= roads[i].GetEnd().x && current_road.GetStart().x >= roads[i].GetStart().x) {
                    // We check whether the new position is located beyond the minimum boundary of the current road
                    if (new_pos.x < current_road.GetMin().x) {
                        SetCurrentRoadsIndex(i);
                    }
                } else if (current_road.GetStart().x <= roads[i].GetStart().x && current_road.GetStart().x >= roads[i].GetEnd().x) {
                    if (new_pos.x < current_road.GetMin().x) {
                        SetCurrentRoadsIndex(i);
                    }
                }
            // Checking for the transition between roads with the same direction
            } else if (current_road.IsHorizontal() && roads[i].IsHorizontal() &&
                       new_pos.y >= roads[i].GetMin().y && new_pos.y <= roads[i].GetMax().y) {
                // We check whether the current road continues to the next one
                if (current_road.GetStart().x == roads[i].GetEnd().x ||
                    current_road.GetEnd().x == roads[i].GetStart().x) {
                        if (new_pos.x < current_road.GetMin().x) {
                            SetCurrentRoadsIndex(i);
                        }
                }
            }
        }
        std::size_t ci = GetCurrentRoadsIndex();
        if (new_pos.x < roads.at(ci).GetMin().x) {
            new_x = roads.at(ci).GetMin().x;
            SetSpeed({0.0, 0.0});
        }
        SetPosition({new_x, new_y});
    }
}

void Dog::SetPositionWhenMovinggEast(const Map::Roads& roads, int delta) {
    double new_x, new_y;
    const double second = 1000.;
    new_x = GetPosition().x + GetSpeed().x * (delta / second);
    new_y = GetPosition().y;
    geom::Point2D new_pos;
    new_pos = {new_x, new_y};

    const auto& current_road = roads.at(GetCurrentRoadsIndex());
    if (IsWithinRoadBounds(new_pos, current_road)) {
        SetPosition({new_pos.x, new_pos.y});
    } else {
        for (std::size_t i = 0; i != roads.size(); ++i) {
            // Checking for the transition between vertical and horizontal roads
            // We check whether the new position is in the range of the next road
            if (current_road.IsVertical() && roads[i].IsHorizontal() &&
                new_pos.y >= roads[i].GetMin().y && new_pos.y <= roads[i].GetMax().y) {
                if (current_road.GetStart().x <= roads[i].GetEnd().x && current_road.GetStart().x >= roads[i].GetStart().x) {
                    // We check whether the new position is located beyond the maximum boundary of the current road
                    if (new_pos.x > current_road.GetMax().x) {
                        SetCurrentRoadsIndex(i);
                    }
                } else if (current_road.GetStart().x <= roads[i].GetStart().x && current_road.GetStart().x >= roads[i].GetEnd().x) {
                    if (new_pos.x > current_road.GetMax().x) {
                        SetCurrentRoadsIndex(i);
                    }
                }
            // Checking for the transition between roads with the same direction
            } else if (current_road.IsHorizontal() && roads[i].IsHorizontal() &&
                       new_pos.y >= roads[i].GetMin().y && new_pos.y <= roads[i].GetMax().y) {
                // We check whether the current road continues to the next one
                if (current_road.GetStart().x == roads[i].GetEnd().x ||
                    current_road.GetEnd().x == roads[i].GetStart().x) {
                        if (new_pos.x > current_road.GetMax().x) {
                            SetCurrentRoadsIndex(i);
                        }
                }
            }
        }
        std::size_t ci = GetCurrentRoadsIndex();
        if (new_pos.x > roads.at(ci).GetMax().x) {
            new_x = roads.at(ci).GetMax().x;
            SetSpeed({0.0, 0.0});
        }
        SetPosition({new_x, new_y});
    }
}

void Dog::SetPositionWhenMovingNorth(const Map::Roads& roads, int delta) {
    double new_x, new_y;
    const double second = 1000.;
    new_x = GetPosition().x;
    new_y = GetPosition().y + GetSpeed().y * (delta / second);
    geom::Point2D new_pos;
    new_pos = {new_x, new_y};

    const auto& current_road = roads.at(GetCurrentRoadsIndex());
    if (IsWithinRoadBounds(new_pos, current_road)) {
        SetPosition({new_pos.x, new_pos.y});
    } else {
        for (std::size_t i = 0; i != roads.size(); ++i) {
            // Checking for the transition between horizontal and vertical roads
            // We check whether the new position is in the range of the next road
            if (current_road.IsHorizontal() && roads[i].IsVertical() &&
                new_pos.x >= roads[i].GetMin().x && new_pos.x <= roads[i].GetMax().x) {
                if (current_road.GetStart().y <= roads[i].GetEnd().y && current_road.GetStart().y >= roads[i].GetStart().y) {
                    // We check whether the new position is located beyond the minimum boundary of the current road
                    if (new_pos.y < current_road.GetMin().y) {
                        SetCurrentRoadsIndex(i);
                    }
                } else if (current_road.GetStart().y <= roads[i].GetStart().y && current_road.GetStart().y >= roads[i].GetEnd().y) {
                    if (new_pos.y < current_road.GetMin().y) {
                        SetCurrentRoadsIndex(i);
                    }
                }
            // Checking for the transition between roads with the same direction
            } else if (current_road.IsVertical() && roads[i].IsVertical() &&
                       new_pos.x >= roads[i].GetMin().x && new_pos.x <= roads[i].GetMax().x) {
                // We check whether the current road continues to the next one
                if (current_road.GetStart().x == roads[i].GetEnd().x ||
                    current_road.GetEnd().x == roads[i].GetStart().x) {
                        if (new_pos.y < current_road.GetMin().y) {
                            SetCurrentRoadsIndex(i);
                        }
                }
            }
        }
        std::size_t ci = GetCurrentRoadsIndex();
        if (new_pos.y < roads.at(ci).GetMin().y) {
            new_y = roads.at(ci).GetMin().y;
            SetSpeed({0.0, 0.0});
        }
        SetPosition({new_x, new_y});
    }
}

void Dog::SetPositionWhenMovingSouth(const Map::Roads& roads, int delta) {
    double new_x, new_y;
    const double second = 1000.;
    new_x = GetPosition().x;
    new_y = GetPosition().y + GetSpeed().y * (delta / second);
    geom::Point2D new_pos;
    new_pos = {new_x, new_y};

    const auto& current_road = roads.at(GetCurrentRoadsIndex());
    if (IsWithinRoadBounds(new_pos, current_road)) {
        SetPosition({new_pos.x, new_pos.y});
    } else {
        for (std::size_t i = 0; i != roads.size(); ++i) {
            // Checking for the transition between horizontal and vertical roads
            // We check whether the new position is in the range of the next road
            if (current_road.IsHorizontal() && roads[i].IsVertical() &&
                new_pos.x >= roads[i].GetMin().x && new_pos.x <= roads[i].GetMax().x) {
                if (current_road.GetStart().y <= roads[i].GetEnd().y && current_road.GetStart().y >= roads[i].GetStart().y) {
                    // We check whether the new position is located beyond the maximum boundary of the current road
                    if (new_pos.y > current_road.GetMax().y) {
                        SetCurrentRoadsIndex(i);
                    }
                } else if (current_road.GetStart().y <= roads[i].GetStart().y && current_road.GetStart().y >= roads[i].GetEnd().y) {
                    if (new_pos.y > current_road.GetMax().y) {
                        SetCurrentRoadsIndex(i);
                    }
                }
            // Checking for the transition between roads with the same direction
            } else if (current_road.IsVertical() && roads[i].IsVertical() &&
                       new_pos.x >= roads[i].GetMin().x && new_pos.x <= roads[i].GetMax().x) {
                // We check whether the current road continues to the next one
                if (current_road.GetStart().x == roads[i].GetEnd().x ||
                    current_road.GetEnd().x == roads[i].GetStart().x) {
                        if (new_pos.y > current_road.GetMax().y) {
                            SetCurrentRoadsIndex(i);
                        }
                }
            }
        }
        std::size_t ci = GetCurrentRoadsIndex();
        if (new_pos.y > roads.at(ci).GetMax().y) {
            new_y = roads.at(ci).GetMax().y;
            SetSpeed({0.0, 0.0});
        }
        SetPosition({new_x, new_y});
    }
}

std::size_t Dog::GetBagCapacity() const noexcept {
    return bag_cap_;
}

Dog::Score Dog::GetScore() const noexcept {
    return score_;
}

[[nodiscard]] bool Dog::PutToBag(FoundObject item) {
    if (IsBagFull()) {
        return false;
    }

    bag_.push_back(item);
    return true;
}

std::size_t Dog::EmptyBag() noexcept {
    auto res = bag_.size();
    bag_.clear();

    return res;
}

bool Dog::IsBagFull() const noexcept {
    return bag_.size() >= bag_cap_;
}

const Dog::BagContent& Dog::GetBagContent() const noexcept {
    return bag_;
}

void Dog::AddScore(Score score) noexcept {
    score_ += score;
}

GameState::GameState(GameSession::DogPtr dog_ptr)
    : current_dog_ptr{dog_ptr} {
}

GameSession::GameSession(const Map& map) noexcept
    : map_{&map} {
}

GameSession::DogPtr GameSession::AddDog(const std::string& name, const geom::Point2D& pos, std::size_t index) {
    std::uint32_t id = next_id_;
    ++next_id_;
    auto dog_ptr = std::make_shared<Dog>(Dog::Id{id}, name, GetMap()->GetBagCapacity());
    dog_ptr->SetPosition(pos);
    dog_ptr->SetSpeed({0., 0.});
    dog_ptr->SetDirection(Dog::Direction::NORTH);
    dog_ptr->SetCurrentRoadsIndex(index);
    dogs_.emplace_back(dog_ptr);
    return dog_ptr;
}

const Map* GameSession::GetMap() const noexcept {
    return map_;
}

const GameSession::Dogs& GameSession::GetDogs() const noexcept {
    return dogs_;
}

const GameSession::GameStateList& GameSession::GetGameStateList() const noexcept {
    return game_state_list_;
}

const GameSession::Items& GameSession::GetItems() const noexcept {
    return items_;
}

void GameSession::SetDogs(const GameSession::Dogs& dogs) {
    dogs_.reserve(dogs.size());
    dogs_ = std::move(dogs);
}

void GameSession::SetItems(const GameSession::Items& items) {
    items_.reserve(items.size());
    items_ = std::move(items);
}

void GameSession::SetNextId(std::uint32_t next_id) {
    next_id_ = next_id;
}

void GameSession::UpdateGameState(int delta) {
    UpdateDogs(delta);
    UpdateLostObjects();
    ProcessGatherEvents();
    ProcessReturnToBaseEvents();
}

void GameSession::SetLocationDogs(const DogPtr& dog_ptr, int delta) {
    const auto& roads = GetMap()->GetRoads();
    const auto& dir = dog_ptr->GetDirection();
    if (dir == model::Dog::Direction::WEST){
        dog_ptr->SetPositionWhenMovingWest(roads, delta);
    } else if (dir == model::Dog::Direction::EAST) {
        dog_ptr->SetPositionWhenMovinggEast(roads, delta);
    } else if (dir == model::Dog::Direction::NORTH) {
        dog_ptr->SetPositionWhenMovingNorth(roads, delta);
    } else if (dir == model::Dog::Direction::SOUTH) {
        dog_ptr->SetPositionWhenMovingSouth(roads, delta);
    }
}

void GameSession::UpdateDogs(int delta) {
    static const double half_width = 0.3;
    for (auto dog_ptr : dogs_) {
        if (dog_ptr) {
            SetLocationDogs(dog_ptr, delta);

            game_state_list_.emplace((*dog_ptr->GetId()), GameState{dog_ptr});

            collision_detector::Gatherer gatherer = {
                                                        dog_ptr->GetPreviousPosition(),
                                                        dog_ptr->GetPosition(),
                                                        half_width
                                                    };

            std::size_t index = static_cast<std::size_t>(*(dog_ptr->GetId()));
            if (std::size_t new_size = index + 1; new_size > gatherers_.size()) {
                gatherers_.resize(new_size);
            }
            gatherers_[index] = std::move(gatherer);
        }
    }
}

void GameSession::UpdateLostObjects() {
    unsigned int looter_count = gatherers_.size();
    const auto& loot = map_->GetLoot();
    const auto loot_count = loot->GetLootCount(looter_count);
    for (unsigned int i = 0u; i < loot_count; ++i) {
        auto type = GetRandomType(loot->GetLootTypesCount());
        auto pos = GetRandomPosition(map_->GetRoads().at(GetRandomIndex(map_->GetRoads().size())));
        if (auto lost_object = loot->AddLostObject(type, pos); lost_object) {
            items_.emplace_back(collision_detector::Item{
                                                            lost_object->GetPosition(),
                                                            0.
                                                        });
        }
    }
}

void GameSession::ProcessGatherEvents() {
    collision_detector::ItemGathererProviderImpl provider(items_, gatherers_);
    auto events = collision_detector::FindGatherEvents(provider);
    if (events.size() > 1) {
        const double epsilon = 1e-10;
        std::sort(events.begin(), events.end(),
            [epsilon](const collision_detector::GatheringEvent& a, const collision_detector::GatheringEvent& b) {
                if (std::abs(a.time - 1) < epsilon && std::abs(b.time - 1) < epsilon) {
                    return a.sq_distance < b.sq_distance;
                }
                if (std::abs(a.time - b.time) < epsilon) {
                    return a.sq_distance < b.sq_distance;
                }
                return a.time > b.time;
            }
        );
    }
    const auto& lost_objects =  map_->GetLoot()->GetLostObjects();
    for (const auto& event : events) {
        if (auto it = std::find_if(lost_objects.begin(), lost_objects.end(),
                                        [&event](const auto& obj_ptr) {
                                            return event.item_id == *(obj_ptr->GetId());
                                        }
            ); it != lost_objects.end()) {
            if (dogs_.at(event.gatherer_id)->PutToBag({model::FoundObject::Id{*((*it)->GetId())}, (*it)->GetType()})) {
                map_->GetLoot()->RemoveLostObject((*it));
            }
        }
    }
}

void GameSession::ProcessReturnToBaseEvents() {
    if (bases_.empty()) {
        for (const auto& office : map_->GetOffices()) {
            bases_.emplace_back(std::move(
                                            collision_detector::Item{
                                                                        {static_cast<double>(office.GetPosition().x),
                                                                        static_cast<double>(office.GetPosition().y)},
                                                                        0.25
                                                                    }));
        }
    }

    collision_detector::ItemGathererProviderImpl provider(bases_, gatherers_);
    for (const auto& event : collision_detector::FindGatherEvents(provider)) {
        auto it = std::find_if(dogs_.begin(), dogs_.end(),
                                        [&event](auto dog_ptr) {
                                            return event.gatherer_id == *(dog_ptr->GetId());
                                        });

        const auto& dog_ptr = *it;
        for (const auto& found_object : dog_ptr->GetBagContent()) {
            dog_ptr->AddScore(map_->GetLoot()->GetValue(found_object.type));
        }
        dog_ptr->EmptyBag();
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

void Game::AddGameSession(const model::Map* map) {
    sessions_.emplace_back(std::make_shared<GameSession>((*map)));
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Game::GameSessions& Game::GetGameSessions() const noexcept {
    return sessions_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

Game::GameSessionPtr Game::FindGameSession(const Map::Id& id) const noexcept {
    auto it = std::find_if(sessions_.begin(), sessions_.end(), [&id](const Game::GameSessionPtr& session) {
        return session->GetMap()->GetId() == id;
    });

    if (it != sessions_.end()) {
        return (*it);
    }
    return nullptr;
}

std::size_t GetRandomIndex(std::size_t count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dis(0, (count - 1));
    std::size_t i = (dis(gen));
    return i;
}

geom::Point2D GetRandomPosition(const Road& road) {
    double x_min = road.GetMin().x;
    double x_max = road.GetMax().x;
    double y_min = road.GetMin().y;
    double y_max = road.GetMax().y;

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<double> x_dis(x_min, x_max);
    std::uniform_real_distribution<double> y_dis(y_min, y_max);

    double random_x, random_y;
    random_x = x_dis(gen);
    random_y = y_dis(gen);
    return {random_x, random_y};
}

unsigned int GetRandomType(unsigned int loot_types_count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(0u, loot_types_count);
    const unsigned int type = (dis(gen));
    return type;
}

}  // namespace model
