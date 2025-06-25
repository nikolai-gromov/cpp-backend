#pragma once

#include <chrono>
#include <functional>

namespace loot_gen {

class LootGenerator {
public:
    using RandomGenerator = std::function<double()>;
    using TimeInterval = std::chrono::milliseconds;

    // base_interval - base time interval > 0
    // probability - probability of a loot appearing during the base time interval
    // random_generator - random number generator in the range [0 to 1]
    LootGenerator(TimeInterval base_interval, double probability,
                  RandomGenerator random_gen = DefaultGenerator)
        : base_interval_{base_interval}
        , probability_{probability}
        , random_generator_{std::move(random_gen)} {
    }

    // Returns the number of loot items that should appear on the map after
    // the specified time interval.
    // The number of loot items appearing on the map does not exceed the number of looters.

    // time_delta - time interval that has passed since the last call to Generate
    // loot_count - number of loot items on the map before calling Generate
    // looter_count - number of looters on the map
    unsigned Generate(TimeInterval time_delta, unsigned loot_count, unsigned looter_count);

private:
    static double DefaultGenerator() noexcept {
        return 1.0;
    };
    TimeInterval base_interval_;
    double probability_;
    TimeInterval time_without_loot_{};
    RandomGenerator random_generator_;
};

}  // namespace loot_gen
