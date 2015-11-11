#include "catch/catch.hpp"
#include <iostream>
#include "../include/hyper_array/hyper_array.hpp"

TEST_CASE("index/overhead", "[overhead]")
{
    const auto overhead = [](const size_t dimensions) -> size_t {
        return dimensions * sizeof(std::ptrdiff_t);    // 1 * std::array
    };

    REQUIRE(sizeof(hyper_array::index<1>) == overhead(1));
    REQUIRE(sizeof(hyper_array::index<2>) == overhead(2));
    REQUIRE(sizeof(hyper_array::index<3>) == overhead(3));
    REQUIRE(sizeof(hyper_array::index<4>) == overhead(4));
    REQUIRE(sizeof(hyper_array::index<5>) == overhead(5));
    REQUIRE(sizeof(hyper_array::index<6>) == overhead(6));
    REQUIRE(sizeof(hyper_array::index<7>) == overhead(7));
    REQUIRE(sizeof(hyper_array::index<8>) == overhead(8));
    REQUIRE(sizeof(hyper_array::index<9>) == overhead(9));
}

TEST_CASE("index/ctor", "[ctor]")
{
    using hyper_array::index;
    constexpr std::size_t dims = 3;
    using value_type = typename hyper_array::index<dims>::value_type;
    const std::array<value_type, dims> zero_array   = {0, 0, 0};
    const std::array<value_type, dims> random_array = {-1, 10, 0};
    const value_type special_number = 42;
    const std::array<value_type, dims> special_array = {special_number, special_number, special_number};

    SECTION("index()")
    {
        const index<dims> idx{};
        REQUIRE(idx.indices() == zero_array);
    }
    SECTION("index(const this_type& other)")
    {
        {
            const index<dims> other{};
            const index<dims> idx{other};
            REQUIRE(idx.indices() == zero_array);
        }
        {
            index<dims> other{};
            std::copy(random_array.begin(), random_array.end(), other.indices().begin());
            REQUIRE(other.indices() == random_array);

            const index<dims> idx{other};
            REQUIRE(idx.indices() == random_array);
            REQUIRE(idx.indices() == other.indices());
        }
    }
    SECTION("index(this_type&& other)")
    {
        {
            index<dims> other{};
            const index<dims> idx{std::move(other)};
            REQUIRE(idx.indices() == zero_array);
        }
        {
            index<dims> other{};
            std::copy(random_array.begin(), random_array.end(), other.indices().begin());
            REQUIRE(other.indices() == random_array);

            const index<dims> idx{std::move(other)};
            REQUIRE(idx.indices() == random_array);
        }
    }
    SECTION("index(const value_type initialValue)")
    {
        const index<dims> idx{special_number};
        REQUIRE(idx.indices() == special_array);
    }
    SECTION("index(const ::std::array<T, Dimensions>& indices)")
    {
        const index<dims> idx{random_array};
        REQUIRE(idx.indices() == random_array);
    }
    SECTION("index(Indices... indices)")
    {
        const index<dims> idx{random_array[0], random_array[1], random_array[2]};
        REQUIRE(idx.indices() == random_array);
    }
}

TEST_CASE("array/overhead", "[overhead]")
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

