
#include <type_traits>

#include "catch/catch.hpp"

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
    const std::array<value_type, dims> zero_array   = {{0, 0, 0}};
    const std::array<value_type, dims> random_array = {{-1, 10, 0}};
    const value_type special_number = 42;
    const std::array<value_type, dims> special_array = {{special_number, special_number, special_number}};

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

TEST_CASE("index/template_args", "[template_args]")
{
    using hyper_array::index;
    constexpr std::size_t dims = 3;

    SECTION("Dimensions")
    {
        const index<dims> idx{};
        REQUIRE(idx.dimensions() == dims);
    }
}

TEST_CASE("index/element_access", "[element_access]")
{
    constexpr std::size_t              dims = 4;
    ::std::array<std::ptrdiff_t, dims> idx_arr{{64, 42, 314, 9000}};
    hyper_array::index<dims>           idx{idx_arr};

    REQUIRE(idx.indices() == idx_arr);

    for (std::size_t i = 0; i < dims; ++i)
    {
        REQUIRE(idx[i] == idx_arr[i]);

        ++idx[i];
        ++idx_arr[i];

        REQUIRE(idx[i] == idx_arr[i]);
    }
}

TEST_CASE("index/iterators", "[iterators]")
{
    // @todo test the iterators
}

TEST_CASE("index/assignment", "[assignment]")
{
    constexpr std::size_t              dims = 4;
    const ::std::array<std::ptrdiff_t, dims> idx_arr{{64, 42, 314, 9000}};
    const hyper_array::index<dims>           src{idx_arr};

    SECTION("from lvalue index")
    {
        hyper_array::index<dims> dst = src;

        REQUIRE(dst.indices() == src.indices());
    }
    SECTION("from rvalue index")
    {
        hyper_array::index<dims> dst = hyper_array::index<dims>{idx_arr[0], idx_arr[1], idx_arr[2], idx_arr[3]};

        REQUIRE(dst.indices() == idx_arr);
    }
    SECTION("from array")
    {
        hyper_array::index<dims> dst = idx_arr;

        REQUIRE(dst.indices() == idx_arr);
    }
}

TEST_CASE("index/comparison", "[comparison]")
{
    constexpr std::size_t                    dims = 4;
    const ::std::array<std::ptrdiff_t, dims> idx_arr{{64, 42, 314, 9000}};

    SECTION("equality")
    {
        const hyper_array::index<dims> src{idx_arr};
        const hyper_array::index<dims> dst{idx_arr};
        REQUIRE      (src == dst);
        REQUIRE_FALSE(src != dst);
        REQUIRE      (src <= dst);
        REQUIRE_FALSE(src <  dst);
        REQUIRE_FALSE(src >  dst);
        REQUIRE      (src >= dst);
    }
    SECTION("total order")
    {
        const hyper_array::index<dims> idx{1, 2, 3, -4};
        const hyper_array::index<dims> other{7, 3, 4, 5};
        REQUIRE_FALSE(idx == other);
        REQUIRE      (idx != other);
        REQUIRE      (idx <= other);
        REQUIRE      (idx <  other);
        REQUIRE_FALSE(idx >  other);
        REQUIRE_FALSE(idx >= other);
    }
    SECTION("no relation, just different")
    {
        const hyper_array::index<dims> idx{-2, 3, 4, -1};
        const hyper_array::index<dims> other{2, -3, -4, 1};
        REQUIRE_FALSE(idx == other);
        REQUIRE      (idx != other);
        REQUIRE_FALSE(idx <= other);
        REQUIRE_FALSE(idx <  other);
        REQUIRE_FALSE(idx >  other);
        REQUIRE_FALSE(idx >= other);
    }
}

TEST_CASE("index/arithmetic", "[arithmetic]")
{
    constexpr std::size_t dims = 4;
    using index_type = hyper_array::index<dims>;
    index_type idx_a{1, 2, 3, 4};
    index_type idx_b{-1, 2, 3, -4};

    SECTION("add to all")
    {
        REQUIRE((idx_a + 3) == (index_type{4, 5, 6, 7}));
    }
    SECTION("substract from all")
    {
        REQUIRE((idx_a - 3) == (index_type{-2, -1, 0, 1}));
    }
    SECTION("add index")
    {
        REQUIRE((idx_a + idx_b) == (index_type{0, 4, 6, 0}));
    }
    SECTION("substract index")
    {
        REQUIRE((idx_a - idx_b) == (index_type{2, 0, 0, 8}));
    }
}
