#pragma once

#include <boost/json.hpp>

#include "../model/model.h"

namespace extra_data {
namespace json = boost::json;

struct Payload {
    std::unordered_map<model::Map::Id, json::array, util::TaggedHasher<model::Map::Id>> map_id_loot_types;
};

}  // namespace extra_data