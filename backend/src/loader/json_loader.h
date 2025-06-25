#pragma once

#include <filesystem>

#include <boost/json.hpp>

#include "../model/model.h"
#include "../util/extra_data.h"

namespace json_loader {

namespace json = boost::json;

void SetRoads(const json::array& json_roads, model::Map& map);

void SetBuildings(const json::array& json_buildings, model::Map& map);

void SetOffices(const json::array& json_offices, model::Map& map);

void FillValues(const json::array& loot_types, std::vector<unsigned int>& values);

model::Game LoadGame(const std::filesystem::path& json_path, extra_data::Payload& payload);

}  // namespace json_loader
