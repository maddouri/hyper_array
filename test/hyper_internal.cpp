#include <numeric>
#include <type_traits>
#include <vector>

#include "catch/catch.hpp"

#include "../include/hyper_array/hyper_array.hpp"

TEST_CASE("enable_if_t", "[type_traits]")
{
    REQUIRE((std::is_same<hyper_array::internal::enable_if_t<true, int>, int>::value));
    /* shouldn't compile */ //REQUIRE_FALSE((std::is_same<hyper_array::internal::enable_if_t<false, int>, int>::value));

    using ref = int&;
    REQUIRE((std::is_same<hyper_array::internal::enable_if_t<true, ref>, ref>::value));
}

TEST_CASE("bool_pack", "[type_traits]")
{
    REQUIRE((std::is_same<hyper_array::internal::bool_pack<true, false, true>, hyper_array::internal::bool_pack<true, false, true>>::value));
    REQUIRE_FALSE((std::is_same<hyper_array::internal::bool_pack<false, true, false>, hyper_array::internal::bool_pack<true, false, true>>::value));
}

TEST_CASE("are_all_same", "[type_traits]")
{
    REQUIRE((hyper_array::internal::are_all_true<true, true, true>::value));
    REQUIRE_FALSE((hyper_array::internal::are_all_true<false, false, false>::value));
    REQUIRE_FALSE((hyper_array::internal::are_all_true<true, false, true>::value));
    REQUIRE_FALSE((hyper_array::internal::are_all_true<false, true, false>::value));
}

TEST_CASE("are_integral", "[type_traits]")
{
    REQUIRE((hyper_array::internal::are_integral<int, int, int>::value));
    REQUIRE((hyper_array::internal::are_integral<int, int&, int>::value));
    REQUIRE_FALSE((hyper_array::internal::are_integral<int, int*, int>::value));
    REQUIRE((hyper_array::internal::are_integral<int, const int, int>::value));
    REQUIRE((hyper_array::internal::are_integral<int, char, unsigned, long>::value));
    REQUIRE((hyper_array::internal::are_integral<int, char&, unsigned, long>::value));
    REQUIRE_FALSE((hyper_array::internal::are_integral<int, char*, unsigned, long>::value));
    REQUIRE_FALSE((hyper_array::internal::are_integral<int, char, unsigned*, long>::value));
    REQUIRE((hyper_array::internal::are_integral<int, char, unsigned&, long>::value));
}

TEST_CASE("ct_plus", "[type_traits]")
{
    REQUIRE(hyper_array::internal::ct_plus(1, 2) == 3);
    REQUIRE(hyper_array::internal::ct_plus(-10.0, 10.0) == 0.0);
    REQUIRE(hyper_array::internal::ct_plus(300000000, 300000000) == 600000000L);
}

TEST_CASE("ct_prod", "[type_traits]")
{
    REQUIRE(hyper_array::internal::ct_prod(1, 2) == 2);
    REQUIRE(Approx(hyper_array::internal::ct_prod(-10.0, 10.0)) == -100.0);
    REQUIRE_FALSE(hyper_array::internal::ct_prod(300000000  , 300000000  ) == 90000000000000000  );
    REQUIRE(      hyper_array::internal::ct_prod(300000000LL, 300000000LL) == 90000000000000000LL);
}

TEST_CASE("ct_accumulate", "[type_traits]")
{
    constexpr size_t    element_count        = 1000;
    constexpr int       series_initial_value = 1;
    constexpr long long series_sum           = element_count * (series_initial_value + element_count) / 2;

    {
        std::array<int, element_count> series;
        std::iota(series.begin(), series.end(), static_cast<int>(series_initial_value));
        REQUIRE(series_sum == hyper_array::internal::ct_accumulate(series, 0, element_count, static_cast<int>(0), hyper_array::internal::ct_plus<int>));
    }

    {
        std::array<double, element_count> series;
        std::iota(series.begin(), series.end(), static_cast<double>(series_initial_value));
        REQUIRE(Approx(series_sum) == hyper_array::internal::ct_accumulate(series, 0, element_count, static_cast<double>(0), hyper_array::internal::ct_plus<double>));
    }
}

TEST_CASE("ct_inner_product", "[type_traits]")
{
    constexpr size_t    element_count  = 3;
    constexpr long long non_null_value = 32;  // value of dot(u, v)

    {
        std::array<int, element_count> u{{1, 2, 3}};
        std::array<int, element_count> v{{4, 5, 6}};
        std::array<int, element_count> w{{-3,6, -3}};
        REQUIRE(non_null_value == hyper_array::internal::ct_inner_product(
                u, 0, v, 0, element_count, static_cast<int>(0),
                hyper_array::internal::ct_plus<int>,
                hyper_array::internal::ct_prod<int>));
        REQUIRE(0 == hyper_array::internal::ct_inner_product(
                u, 0, w, 0, element_count, static_cast<int>(0),
                hyper_array::internal::ct_plus<int>,
                hyper_array::internal::ct_prod<int>));
    }

    {
        std::array<double, element_count> u{{1, 2, 3}};
        std::array<double, element_count> v{{4, 5, 6}};
        std::array<double, element_count> w{{-3,6, -3}};
        REQUIRE(Approx(non_null_value) == hyper_array::internal::ct_inner_product(
                u, 0, v, 0, element_count, static_cast<double>(0.0),
                hyper_array::internal::ct_plus<double>,
                hyper_array::internal::ct_prod<double>));
        REQUIRE(Approx(0.0) == hyper_array::internal::ct_inner_product(
                u, 0, w, 0, element_count, static_cast<double>(0.0),
                hyper_array::internal::ct_plus<double>,
                hyper_array::internal::ct_prod<double>));
    }
}













