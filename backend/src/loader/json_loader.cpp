#include "json_loader.h"

#include <iostream>
#include <fstream>

namespace json_loader {

void SetRoads(const json::array& json_roads, model::Map& map) {
    using namespace std::literals;
    for (const auto& json_road : json_roads) {
        model::Point start;
        start.x = json_road.as_object().at("x0").as_int64();
        start.y = json_road.as_object().at("y0").as_int64();
        if (auto search = json_road.as_object().find("x1"); search != json_road.as_object().end()) {
            model::Coord end_x;
            end_x = json_road.as_object().at("x1").as_int64();
            model::Road road(model::Road::Direction::HORIZONTAL, {start.x, start.y}, end_x);
            map.AddRoad(road);
        } else {
            model::Coord end_y;
            end_y = json_road.as_object().at("y1").as_int64();
            model::Road road(model::Road::Direction::VERTICAL, {start.x, start.y}, end_y);
            map.AddRoad(road);
        }
    }
}

void SetBuildings(const json::array& json_buildings, model::Map& map) {
    model::Point position;
    model::Size size;
    for (const auto& json_building : json_buildings) {
        position.x = json_building.as_object().at("x").as_int64();
        position.y = json_building.as_object().at("y").as_int64();
        size.width = json_building.as_object().at("w").as_int64();
        size.height = json_building.as_object().at("h").as_int64();
        model::Building building({position, size});
        map.AddBuilding(building);
    }
}

void SetOffices(const json::array& json_offices, model::Map& map) {
    std::string id;
    model::Point position;
    model::Offset offset;
    for (const auto& json_office : json_offices) {
        id = json_office.as_object().at("id").as_string().c_str();
        position.x = json_office.as_object().at("x").as_int64();
        position.y = json_office.as_object().at("y").as_int64();
        offset.dx = json_office.as_object().at("offsetX").as_int64();
        offset.dy = json_office.as_object().at("offsetY").as_int64();
        model::Office office(model::Office::Id(id), position, offset);
        map.AddOffice(office);
    }
}

void FillValues(const json::array& loot_types, std::vector<unsigned int>& values) {
    for (const auto& loot_type : loot_types) {
        unsigned int value = loot_type.as_object().at("value").as_int64();
        values.emplace_back(value);
    }
}

model::Game LoadGame(const std::filesystem::path& json_path, extra_data::Payload& payload) {
    using namespace std::literals;

    std::ifstream json_file(json_path, std::ios::binary);
    if (!json_file.is_open()) {
        throw std::runtime_error("Failed to open file"s + json_path.string());
    }

    std::string json_string((std::istreambuf_iterator<char>(json_file)), std::istreambuf_iterator<char>());
    json::value json_value = json::parse(json_string);
    model::Game game;

    double default_dog_speed = 1.;
    if (json_value.as_object().contains("defaultDogSpeed")) {
        default_dog_speed = json_value.as_object().at("defaultDogSpeed").as_double();
    }

    double period, probability;
    if (json_value.as_object().contains("lootGeneratorConfig")) {
        json::object loot_config = json_value.as_object().at("lootGeneratorConfig").as_object();
        period = loot_config.at("period").as_double();
        probability = loot_config.at("probability").as_double();
    }

    size_t default_bag_capacity;
    if (json_value.as_object().contains("defaultBagCapacity")) {
        default_bag_capacity = json_value.as_object().at("defaultBagCapacity").as_int64();
    }

    json::array json_maps = json_value.as_object().at("maps").as_array();
    for (const auto& json_map : json_maps){
        std::string id = json_map.as_object().at("id").as_string().c_str();
        std::string name = json_map.as_object().at("name").as_string().c_str();

        double dog_speed = default_dog_speed;
        if (json_map.as_object().contains("dogSpeed")) {
            dog_speed = json_map.as_object().at("dogSpeed").as_double();
        }

        size_t bag_capacity = default_bag_capacity;
        if (json_map.as_object().contains("bagCapacity")) {
            bag_capacity = json_map.as_object().at("bagCapacity").as_int64();
        }

        unsigned loot_types_count;
        std::vector<unsigned> values;
        if (json_map.as_object().contains("lootTypes")) {
            json::array loot_types = json_map.as_object().at("lootTypes").as_array();
            loot_types_count = loot_types.size() - 1;
            payload.map_id_loot_types.emplace(model::Map::Id(id), loot_types);

            FillValues(loot_types, values);
        }

        model::Map map(model::Map::Id(id), name, dog_speed, bag_capacity);

        json::array json_roads = json_map.as_object().at("roads").as_array();
        SetRoads(json_roads, map);

        json::array json_buildings = json_map.as_object().at("buildings").as_array();
        SetBuildings(json_buildings, map);

        json::array json_offices = json_map.as_object().at("offices").as_array();
        SetOffices(json_offices, map);

        map.AddLoot({period, probability}, loot_types_count, values);

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
