#pragma once

#include <algorithm>
#include <vector>

#include "../util/geom.h"

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        const double epsilon = 1e-10;
        if (proj_ratio < -epsilon || proj_ratio > 1 + epsilon) {
            return false;
        }
        return sq_distance <= (collect_radius * collect_radius + epsilon);
    }

    double sq_distance;
    double proj_ratio;
};

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    geom::Point2D position;
    double width;
};

struct Gatherer {
    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

class ItemGathererProviderImpl : public ItemGathererProvider {
public:
    ItemGathererProviderImpl(const std::vector<Item>& items, const std::vector<Gatherer>& gatherers);

    size_t ItemsCount() const override;

    Item GetItem(size_t idx) const override;

    size_t GatherersCount() const override;

    Gatherer GetGatherer(size_t idx) const override;

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector