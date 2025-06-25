#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include "../src/detector/collision_detector.h"

namespace Catch {

using namespace collision_detector;

template<>
struct StringMaker<GatheringEvent> {
  static std::string convert(GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

      return tmp.str();
  }
};

struct Comparator {
    bool operator()(const GatheringEvent& lhs, const GatheringEvent& rhs) const {
        return lhs.item_id == rhs.item_id &&
           lhs.gatherer_id == rhs.gatherer_id &&
           std::abs(lhs.sq_distance - rhs.sq_distance) < 1e-10 &&
           std::abs(lhs.time - rhs.time) < 1e-10;
    }
};

template<typename Range>
struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase {
    EqualsRangeMatcher(Range const& range)
        : range_{range} {
    }

    template<typename OtherRange>
    bool match(OtherRange const& other) const {
        using std::begin; using std::end;

        return std::equal(begin(range_), end(range_), begin(other), end(other), Comparator{});
    }

    std::string describe() const override {
        return "Equals: " + Catch::rangeToString(range_);
    }

private:
    Range const& range_;
};

template<typename Range>
auto EqualsRange(const Range& range) -> EqualsRangeMatcher<Range> {
    return EqualsRangeMatcher<Range>{range};
}

TEST_CASE("FindGatherEvents function tests") {
    std::vector<Item> items = { { {0.4, 0.6}, 0.0 },
                                { {0.6, 0.0}, 0.0 },
                                { {39.4, 0.7}, 0.0 },
                                { {10.0, 10.4}, 0.0 },
                                { {29.7, 39.9}, 0.3 },
                                { {3.0, 2.0}, 1.4 },
                                { {3.0, 6.0}, 1.4 } };

    std::vector<Gatherer> gatherers = { { {0.0, 0.5}, {0.0, 0.5}, 0.6 },
                                        { {0.4, 0.0}, {0.4, 0.0}, 0.6 },
                                        { {0.0, 0.0}, {0.5, 0.0}, 0.6 },
                                        { {0.3, 0.2}, {1.5, 0.2}, 0.6 },
                                        { {1.2, 0.0}, {0.0, 0.0}, 0.6 },
                                        { {0.0, 0.8}, {0.0, 0.0}, 0.6 },
                                        { {0.0, 0.0}, {0.0, 0.0}, 0.6 },
                                        { {39.0, 0.0}, {39.5, 0.0}, 0.6 },
                                        { {10.0, 10.0}, {10.0, 10.8}, 0.6 },
                                        { {10.0, 11.0}, {10.0, 10.5}, 0.6 },
                                        { {10.0, 9.9}, {10.0, 10.7}, 0.6 },
                                        { {15.0, 10.0}, {15.0, 10.5}, 0.6 },
                                        { {10.0, 10.0}, {10.0, 9.5}, 0.6 },
                                        { {29.2, 39.0}, {29.9, 39.0}, 0.6 },
                                        { {1.0, 1.0}, {5.0, 5.0}, 0.6 } };

    ItemGathererProviderImpl provider(items, gatherers);

    std::vector<GatheringEvent> events = FindGatherEvents(provider);

    std::vector<GatheringEvent> expected_events = { { 0, 3, 0.16, 0.083333333333333356 },
                                                    { 1, 3, 0.04, 0.25 },
                                                    { 0, 5, 0.16, 0.25 },
                                                    { 5, 14, 0.5, 0.375 },
                                                    { 1, 4, 0., 0.5 },
                                                    { 3, 8, 0., 0.5 },
                                                    { 3, 10, 0., 0.62500000000000078 },
                                                    { 0, 4, 0.36, 0.66666666666666663 },
                                                    { 4, 13, 0.81, 0.71428571428571508 },
                                                    { 0, 2, 0.36, 0.80000000000000004 },
                                                    { 1, 5, 0.36, 1. }};

    REQUIRE_THAT(events, EqualsRange(expected_events));

    std::cout << "FindGatherEvents: ";
    for (const auto& event : events) {
        std::cout << StringMaker<GatheringEvent>::convert(event) << " ";
    }
    std::cout << std::endl;
}

}  // namespace Catch