#include "catch/catch.hpp"

#include "../include/hyper_array/hyper_array.hpp"

TEST_CASE("overhead", "[overhead]")
{
    using value_type = double;
    const auto overhead = [](const size_t dimensions) -> size_t {
        return 2 * dimensions * sizeof(std::size_t)    // 2 * std::array
             + sizeof(size_t)                          // 1 * _size
             + sizeof(std::unique_ptr<value_type[]>);  // 1 * unique_ptr
    };

    REQUIRE(sizeof(hyper_array::array<value_type, 1>) == overhead(1));
    REQUIRE(sizeof(hyper_array::array<value_type, 2>) == overhead(2));
    REQUIRE(sizeof(hyper_array::array<value_type, 3>) == overhead(3));
    REQUIRE(sizeof(hyper_array::array<value_type, 4>) == overhead(4));
    REQUIRE(sizeof(hyper_array::array<value_type, 5>) == overhead(5));
    REQUIRE(sizeof(hyper_array::array<value_type, 6>) == overhead(6));
    REQUIRE(sizeof(hyper_array::array<value_type, 7>) == overhead(7));
    REQUIRE(sizeof(hyper_array::array<value_type, 8>) == overhead(8));
    REQUIRE(sizeof(hyper_array::array<value_type, 9>) == overhead(9));

}
