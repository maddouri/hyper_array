#pragma once

// make sure that -std=c++11 or -std=c++14 ... is enabled in case of clang and gcc
#if (__cplusplus < 201103L)  // C++11 ?
    #error "hyper_array requires a C++11-compliant compiler"
#endif

// <editor-fold desc="Configuration">
#ifndef HYPER_ARRAY_CONFIG_Overload_Stream_Operator
/// Enables/disables `operator<<()` overloading for hyper_array::array
#define HYPER_ARRAY_CONFIG_Overload_Stream_Operator 1
#endif
// </editor-fold>

// <editor-fold desc="Includes">
// std
#include <algorithm>         // std::accumulate, std::equal...
#include <array>             // std::array for hyper_array::array::dimensionLengths and indexCoeffs
#include <cassert>           // assert()
#include <cstddef>           // ptrdiff_t
#include <initializer_list>  // std::initializer_list for the constructors
#include <memory>            // std::unique_ptr, std::addressof
#include <sstream>           // stringstream in hyper_array::array::validateIndexRanges()
#include <type_traits>       // template metaprogramming stuff in hyper_array::internal
#if HYPER_ARRAY_CONFIG_Overload_Stream_Operator
#include <iterator>          // std::ostream_iterator in operator<<()
#include <ostream>           // std::ostream for the overloaded operator<<()
#endif
// </editor-fold>


/// The hyper_array lib's namespace
namespace hyper_array
{

/// represents the array (storage) order
/// @see https://en.wikipedia.org/wiki/Row-major_order
enum class array_order : int
{
    ROW_MAJOR    = 0,  ///< a.k.a. C-style order
    COLUMN_MAJOR = 1   ///< a.k.a. Fortran-style order
};

/// used for tag dispatching
template <array_order Order> struct array_order_tag {};

// <editor-fold defaultstate="collapsed" desc="Forward Declarations">
template <std::size_t Dimensions>                                                      class index;
template <std::size_t Dimensions>                                                      class bounds;
template <typename ValueType, std::size_t Dimensions, array_order Order, bool IsConst> class iterator;
template <typename ValueType, std::size_t Dimensions, array_order Order, bool IsConst> class view;
template <typename ValueType, std::size_t Dimensions, array_order Order>               class array;
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="Internal Helper Blocks">
/// helper functions for hyper_array::array's implementation
/// @note Everything here is subject to change and must NOT be used by user code
namespace internal
{

/// shorthand for the enable_if syntax
/// @see http://en.cppreference.com/w/cpp/types/enable_if#Helper_types
template <bool b, typename T>
using enable_if_t = typename std::enable_if<b, T>::type;

/// shorthand for the std::conditional syntax
/// @see http://en.cppreference.com/w/cpp/types/conditional
template <bool b, typename T, typename F>
using conditional_t = typename std::conditional<b, T, F>::type;

/// building block of a neat trick for checking multiple types against a given trait
template <bool...>
struct bool_pack
{};

/// neat trick for checking multiple types against a given trait
/// https://codereview.stackexchange.com/a/107903/86688
template <bool... bs>
using are_all_true = std::is_same<bool_pack<true, bs...>,
                                  bool_pack<bs..., true>>;

/// checks that all the template arguments are integral types
/// @note `T&` where `std::is_integral<T>::value==true` is considered integral
/// by removing any reference then using `std::is_integral`
template <typename... Ts>
using are_integral = are_all_true<
    std::is_integral<
        typename std::remove_reference<Ts>::type
    >::value...
>;

/// avoids repetion/long lines in a few enable_if_t expressions
template <std::size_t Dimensions, typename... Indices>
using are_indices = std::integral_constant<bool,
                                           (sizeof...(Indices) == Dimensions)
                                           && are_integral<Indices...>::value>;

/// compile-time sum
template <typename T, typename U>
inline constexpr typename std::common_type<T, U>::type ct_plus(const T x, const U y)
{
    using TU = typename std::common_type<T, U>::type;
    return static_cast<TU>(x) + static_cast<TU>(y);
}

/// compile-time product
template <typename T, typename U>
inline constexpr typename std::common_type<T, U>::type ct_prod(const T x, const U y)
{
    using TU = typename std::common_type<T, U>::type;
    return static_cast<TU>(x) * static_cast<TU>(y);
}

/// compile-time equivalent to `std::accumulate()`
template <
    typename    T,  ///< result type
    std::size_t N,  ///< length of the array
    typename    O   ///< type of the binary operation
>
inline constexpr
T ct_accumulate(const ::std::array<T, N>& arr,  ///< accumulate from this array
                const size_t first,             ///< starting from this position
                const size_t length,            ///< accumulate this number of elements
                const T      initialValue,      ///< let this be the accumulator's initial value
                const O&     op                 ///< use this binary operation
               )
{
    // https://stackoverflow.com/a/33158265/865719
    return (first < (first + length))
         ? op(arr[first],
              ct_accumulate(arr,
                            first + 1,
                            length - 1,
                            initialValue,
                            op))
         : initialValue;
}

/// compile-time equivalent to `std::inner_product()`
template <
    typename T,      ///< the result type
    typename T_1,    ///< first array's type
    size_t   N_1,    ///< length of the first array
    typename T_2,    ///< second array's type
    size_t   N_2,    ///< length of the second array
    typename O_SUM,  ///< summation operation's type
    typename O_PROD  ///< multiplication operation's type
>
inline constexpr
T ct_inner_product(const ::std::array<T_1, N_1>& arr_1,  ///< calc the inner product of this array
                   const size_t  first_1,                ///< from this position
                   const ::std::array<T_2, N_2>& arr_2,  ///< with this array
                   const size_t  first_2,                ///< from this position
                   const size_t  length,         ///< using this many elements from both arrays
                   const T       initialValue,   ///< let this be the summation's initial value
                   const O_SUM&  op_sum,         ///< use this as the summation operator
                   const O_PROD& op_prod         ///< use this as the multiplication operator
                  )
{
    // same logic as `ct_accumulate()`
    return (first_1 < (first_1 + length))
         ? op_sum(op_prod(arr_1[first_1],
                          arr_2[first_2]),
                  ct_inner_product(arr_1, first_1 + 1,
                                   arr_2, first_2 + 1,
                                   length - 1,
                                   initialValue,
                                   op_sum, op_prod))
         : initialValue;
}

/// computes the index coefficients given a specific "Order"
/// row-major order
template <typename size_type, std::size_t Dimensions, array_order Order>
inline
enable_if_t<
    Order == array_order::ROW_MAJOR,
    ::std::array<size_type, Dimensions>>
computeIndexCoeffs(const ::std::array<size_type, Dimensions>& dimensionLengths) noexcept
{
    /* doc-style comment block disabled because doxygen/doxypress can't handle it
       just copy/paste into : https://www.codecogs.com/latex/eqneditor.php
        \f[
            \begin{cases}
            C_i = \prod_{j=i+1}^{n-1} L_j
            \\
            \begin{cases}
                i   &\in [0, \text{Dimensions - 1}] \\
                C_i &: \text{\_coeffs[i]}           \\
                L_j &: \text{\_lengths[j]}
            \end{cases}
            \end{cases}
        \f]
    */
    ::std::array<size_type, Dimensions> coeffs;
    for (size_type i = 0; i < Dimensions; ++i)
    {
        coeffs[i] = internal::ct_accumulate(dimensionLengths,
                                            i + 1,
                                            Dimensions - i - 1,
                                            static_cast<size_type>(1),
                                            ct_prod<size_type, size_type>);
    }
    return coeffs;
}

/// computes the index coefficients given a specific "Order"
/// column-major order
template <typename size_type, std::size_t Dimensions, array_order Order>
inline
enable_if_t<
    Order == array_order::COLUMN_MAJOR,
    ::std::array<size_type, Dimensions>>
computeIndexCoeffs(const ::std::array<size_type, Dimensions>& dimensionLengths) noexcept
{
    /* doc-style comment block disabled because doxygen/doxypress can't handle it
       just copy/paste into : https://www.codecogs.com/latex/eqneditor.php
        \f[
            \begin{cases}
            C_i = \prod_{j=0}^{i-1} L_j
            \\
            \begin{cases}
                i   &\in [0, \text{Dimensions - 1}] \\
                C_i &: \text{\_coeffs[i]}           \\
                L_j &: \text{\_lengths[j]}
            \end{cases}
            \end{cases}
        \f]
    */
    ::std::array<size_type, Dimensions> coeffs;
    for (size_type i = 0; i < Dimensions; ++i)
    {
        coeffs[i] = internal::ct_accumulate(dimensionLengths,
                                            0,
                                            i,
                                            static_cast<size_type>(1),
                                            ct_prod<size_type, size_type>);
    }
    return coeffs;
}

/// computes the "flat" or "linear" or "1D" index from the provided index tuple @p indexArray
/// and the index coefficients @p coeffs
template <std::size_t Dimensions, typename size_type, typename flat_index_type>
inline constexpr
size_type
flatten_index(const ::std::array<flat_index_type, Dimensions>& indexArray,
              const ::std::array<size_type, Dimensions>&       coeffs) noexcept
{
    /* https://www.codecogs.com/latex/eqneditor.php
       \begin{cases}
       I_{actual} &= \sum_{i=0}^{N-1} {C_i \cdot I_i}                  \\
                                                                       \\
       I_{actual} &: \text{actual index of the data in the data array} \\
       N          &: \text{Dimensions}                                 \\
       C_i        &: \text{\_coeffs[i]}                                \\
       I_i        &: \text{indexArray[i]}
       \end{cases}
    */
    return internal::ct_inner_product(coeffs, 0,
                                      indexArray, 0,
                                      Dimensions,
                                      static_cast<size_type>(0),
                                      internal::ct_plus<size_type, size_type>,
                                      internal::ct_prod<size_type, flat_index_type>);
}

template <std::size_t Dimensions, typename Iterable>
std::size_t compute_flat_range(const Iterable& hyperRange_)
{
    // the "flat range" is the product of the sub ranges
    return std::accumulate(hyperRange_.begin(), hyperRange_.end(),
                           static_cast<std::size_t>(1),
                           std::multiplies<ptrdiff_t>());
}

template <std::size_t Dimensions, typename Iterable>
std::size_t compute_flat_range(const Iterable& begin_, const Iterable& end_)
{
    // compute the range of each dimension
    ::std::array<std::size_t, Dimensions> ranges;
    std::transform(end_.begin(), end_.end(),
                   begin_.begin(),
                   ranges.begin(),
                   std::minus<ptrdiff_t>());
    //
    return compute_flat_range<Dimensions>(ranges);
}

template <typename T_DST, typename T_SRC, std::size_t LEN>
::std::array<T_DST, LEN> std_array_cast(const ::std::array<T_SRC, LEN>& src)
{
    ::std::array<T_DST, LEN> dst;
    std::copy(src.begin(), src.end(), dst.begin());
    return dst;
}

template <std::size_t Dimensions>
void advance_cursor_dispatch(const array_order_tag<array_order::COLUMN_MAJOR>&,
                             index<Dimensions>& _cursor,
                             const std::ptrdiff_t distance_to_origin,
                             const index<Dimensions>& _begin,
                             const index<Dimensions>& _end)
{
    // basic algorithm: here for reference
//        // range = _end - _begin
//        ::std::array<difference_type, Dimensions> range;
//        std::transform(_end.begin(), _end.end(),
//                       _begin.begin(),
//                       range.begin(),
//                       std::minus<difference_type>());
//
//        // compute the offset of each "component" w.r.t. _begin (the offset is the remainder)
//        ::std::array<difference_type, Dimensions> r;  // remainder
//        ::std::array<difference_type, Dimensions> q;  // quotient
//        q[0] = distance_to_origin;
//        r[0] = q[0] % range[0];
//        for (size_t i = 1; i < Dimensions; ++i)
//        {
//            q[i] = q[i - 1] / range[i - 1];
//            if (q[i] == 0)
//            {
//                std::fill(r.begin() + i, r.end(), 0);
//                break;
//            }
//            r[i] = q[i] % range[i];
//        }
//
//        // _cursor = _begin + r
//        std::transform(_begin.begin(), _begin.end(),
//                       r.begin(),
//                       _cursor.begin(),
//                       std::plus<difference_type>());

    // implementation 2: simplification/refactoring of the "basic algorithm"
    std::ptrdiff_t range;
    std::ptrdiff_t q = distance_to_origin;

    _cursor = _begin;
    for (std::size_t i = 0; i < Dimensions; ++i)
    {
        range = _end[i] - _begin[i];
        _cursor[i] += q % range;
        q /= range;
        if (q == 0)
        {
            break;
        }
    }
}

template <std::size_t Dimensions>
void advance_cursor_dispatch(const array_order_tag<array_order::ROW_MAJOR>&,
                             index<Dimensions>& _cursor,
                             const std::ptrdiff_t distance_to_origin,
                             const index<Dimensions>& _begin,
                             const index<Dimensions>& _end)
{
    // implementation 2: simplification/refactoring of the "basic algorithm"
    std::ptrdiff_t range;
    std::ptrdiff_t q = distance_to_origin;

    _cursor = _begin;
    for (std::ptrdiff_t i = Dimensions - 1; i >= 0; --i)
    {
        const std::size_t ui = static_cast<std::size_t>(i);
        range = _end[ui] - _begin[ui];
        _cursor[ui] += q % range;
        q /= range;
        if (q == 0)
        {
            break;
        }
    }
}

template <std::size_t Dimensions, typename D, typename R>
std::ptrdiff_t cursor_distance_to_origin_dispatch(const array_order_tag<array_order::COLUMN_MAJOR>&,
                                                  const ::std::array<D, Dimensions>& diff,
                                                  const ::std::array<R, Dimensions>& ranges)
{
    std::ptrdiff_t d = 0;
    d += diff[0];
    for (std::size_t i = 1; i < Dimensions; ++i)
    {
        d += diff[i] * std::accumulate(ranges.begin(),
                                       ranges.begin() + i,
                                       static_cast<std::ptrdiff_t>(1),
                                       std::multiplies<std::ptrdiff_t>());
    }
    return d;
}

template <std::size_t Dimensions, typename D, typename R>
std::ptrdiff_t cursor_distance_to_origin_dispatch(const array_order_tag<array_order::ROW_MAJOR>&,
                                                  const ::std::array<D, Dimensions>& diff,
                                                  const ::std::array<R, Dimensions>& ranges)
{
    std::ptrdiff_t d = 0;
    d += diff[Dimensions - 1];
    for (std::size_t i = 1; i < Dimensions; ++i)
    {
        d += diff[Dimensions - 1 - i] * std::accumulate(ranges.rbegin(),
                                                        ranges.rbegin()
                                                        + static_cast<std::ptrdiff_t>(i),
                                                        static_cast<std::ptrdiff_t>(1),
                                                        std::multiplies<std::ptrdiff_t>{});
    }
    return d;
}

template <array_order Order, std::size_t Dimensions, typename D, typename R>
std::ptrdiff_t cursor_distance_to_origin(const ::std::array<D, Dimensions>& diff,    // i.e. cursor
                                         const ::std::array<R, Dimensions>& ranges)  // i.e. end
{
    return cursor_distance_to_origin_dispatch(array_order_tag<Order>{}, diff, ranges);
}

template <array_order Order, std::size_t Dimensions>
std::ptrdiff_t cursor_distance_to_origin(const index<Dimensions>& cursor,
                                         const index<Dimensions>& begin,
                                         const index<Dimensions>& end)
{
    return cursor_distance_to_origin<Order>((cursor - begin)._indices, (end - begin)._indices);
}

template <array_order Order, std::size_t Dimensions>
std::ptrdiff_t cursor_distance_to_origin(const index<Dimensions>& cursor,
                                         const index<Dimensions>& end)
{
    return cursor_distance_to_origin<Order>(cursor._indices, end._indices);
}


}
// </editor-fold>


/// A multi-dimensional index -- i.e. a "Dimensions"-tuple of scalar indices
/// inspired by [Multidimensional bounds, offset and array_view, revision 7](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4512.html)
template <std::size_t Dimensions>
class index
{
public:
    // types
    using this_type              = index<Dimensions>;

    using value_type             = std::ptrdiff_t;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using size_type              = std::size_t;

    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    /// the "Dimensions"-tuple of indices
    ::std::array<value_type, Dimensions> _indices;

public:

    // ctors
    index()
    : index(0)
    {}

    index(const this_type& other)
    : _indices(other._indices)
    {}

    index(this_type&& other)
    : _indices(std::move(other._indices))
    {}

    explicit
    index(const value_type initialValue)
    : _indices([&initialValue](){
        ::std::array<value_type, Dimensions> indices;
        indices.fill(initialValue);
        return indices;
    }())
    {}

    template <typename T>
    index(const ::std::array<T, Dimensions>& indices)
    : _indices([&indices]() {
        decltype(_indices) result;
        std::copy(indices.begin(), indices.end(), result.begin());
        return result;
    }())
    {}

    template <
        typename... Indices,
        typename = internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                                         void>
    >
    index(Indices... indices)
    : _indices{{static_cast<value_type>(indices)...}}
    {}

    /// returns the number of dimensions associated with this index
    static constexpr size_type dimensions() { return Dimensions; }

    // accessors, return the i'th index
          value_type& operator[](size_type i)       { return _indices[i]; }
    const value_type& operator[](size_type i) const { return _indices[i]; }

    // iterators
          iterator         begin()         noexcept { return _indices.begin();   }
    const_iterator         begin()   const noexcept { return _indices.begin();   }
          iterator         end()           noexcept { return _indices.end();     }
    const_iterator         end()     const noexcept { return _indices.end();     }
          reverse_iterator rbegin()        noexcept { return _indices.rbegin();  }
    const_reverse_iterator rbegin()  const noexcept { return _indices.rbegin();  }
          reverse_iterator rend()          noexcept { return _indices.rend();    }
    const_reverse_iterator rend()    const noexcept { return _indices.rend();    }
    const_iterator         cbegin()  const noexcept { return _indices.cbegin();  }
    const_iterator         cend()    const noexcept { return _indices.cend();    }
    const_reverse_iterator crbegin() const noexcept { return _indices.crbegin(); }
    const_reverse_iterator crend()   const noexcept { return _indices.crend();   }

    // operator overloading

    this_type& operator=(const this_type& other)
    {
        std::copy(other._indices.begin(), other._indices.end(), _indices.begin());
        return *this;
    }
    this_type& operator=(this_type&& other)
    {
        _indices = std::move(other._indices);
        return *this;
    }

    bool operator==(const this_type& other) const { return _indices == other._indices; }
    bool operator!=(const this_type& other) const { return _indices != other._indices; }
    bool operator< (const this_type& other) const {
        return std::equal(_indices.begin(), _indices.end(), other._indices.begin(),
                          [](const value_type& a, const value_type& b) { return a < b; });
    }
    bool operator<=(const this_type& other) const {
        return std::equal(_indices.begin(), _indices.end(), other._indices.begin(),
                          [](const value_type& a, const value_type& b) { return a <= b; });
    }
    bool operator> (const this_type& other) const {
        return std::equal(_indices.begin(), _indices.end(), other._indices.begin(),
                          [](const value_type& a, const value_type& b) { return a > b; });
    }
    bool operator>=(const this_type& other) const {
        return std::equal(_indices.begin(), _indices.end(), other._indices.begin(),
                          [](const value_type& a, const value_type& b) { return a >= b; });
    }

    // increment to all
    this_type operator+(const ptrdiff_t d) const
    {
        this_type result;
        std::transform(_indices.begin(), _indices.end(),
                       result._indices.begin(),
                       [&d](const value_type& val) {
                           return val + d;
                       });
        return result;
    }
    // decrement all
    this_type operator-(const ptrdiff_t d) const
    {
        return (*this) + (-d);
    }

    this_type operator+(const this_type& other) const
    {
        this_type result;
        std::transform(_indices.begin(), _indices.end(),
                       other._indices.begin(),
                       result._indices.begin(),
                       std::plus<value_type>{});
        return result;
    }

    this_type operator-(const this_type& other) const
    {
        this_type result;
        std::transform(_indices.begin(), _indices.end(),
                       other._indices.begin(),
                       result._indices.begin(),
                       std::minus<value_type>{});
        return result;
    }
};

/// Represents the lower and upper bounds of a multidimensional range
/// inspired by [Multidimensional bounds, offset and array_view, revision 7](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4512.html)
template <std::size_t Dimensions>
class bounds
{
public:
    /// represents a single dimensional [min, max] range
    struct pair
    {
        using value_type = typename index<Dimensions>::value_type;
        value_type min;
        value_type max;
    };
    // types
    using size_type           = std::size_t;
    using value_type          = pair;
//    using reference           = value_type&;
//    using const_reference     = const pair&;

    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    ::std::array<value_type, Dimensions> _bounds;

public:

    // ctors
    bounds()
    : _bounds([](){
        ::std::array<value_type, Dimensions> bs;
        bs.fill({});
        return bs;
    }())
    {}

    bounds(std::initializer_list<pair> bs)
    : _bounds([&bs](){
        assert(bs.size() == Dimensions);
        ::std::array<value_type, Dimensions> bnds;
        std::copy(bs.begin(), bs.end(), bnds.begin());
        return bnds;
    }())
    {}

    bounds(const index<Dimensions>& lowerBound, const index<Dimensions>& upperBound)
    : _bounds([this, &lowerBound, &upperBound](){
        ::std::array<pair, Dimensions> bs;
        // zip lowerBound and upperBound into bs
        std::transform(lowerBound.begin(), lowerBound.end(),
                       upperBound.begin(),
                       bs.begin(),
                       [](decltype(*lowerBound.begin()) low,
                          decltype(*upperBound.begin()) up) -> value_type {
                           return {low, up};
                       });
        return bs;
    }())
    {}

    // accessors, return the i'th range
          value_type& operator[](size_type i)       { return _bounds[i]; }
    const value_type& operator[](size_type i) const { return _bounds[i]; }

    // iterators
          iterator         begin()         noexcept { return _bounds.begin();   }
    const_iterator         begin()   const noexcept { return _bounds.begin();   }
          iterator         end()           noexcept { return _bounds.end();     }
    const_iterator         end()     const noexcept { return _bounds.end();     }
          reverse_iterator rbegin()        noexcept { return _bounds.rbegin();  }
    const_reverse_iterator rbegin()  const noexcept { return _bounds.rbegin();  }
          reverse_iterator rend()          noexcept { return _bounds.rend();    }
    const_reverse_iterator rend()    const noexcept { return _bounds.rend();    }
    const_iterator         cbegin()  const noexcept { return _bounds.cbegin();  }
    const_iterator         cend()    const noexcept { return _bounds.cend();    }
    const_reverse_iterator crbegin() const noexcept { return _bounds.crbegin(); }
    const_reverse_iterator crend()   const noexcept { return _bounds.crend();   }
};

/// Iterates over the elements of a hyper array
/// that are comprised within the limits of a given hyper view
/// NB: in order to avoid code duplication for the "const" version of the iterator
///     I'm going to use a technique explained on Dr. Dobb's http://www.ddj.com/cpp/184401331
///     other ideas can be found on SO https://stackoverflow.com/q/2150192/865719
template <
    typename    ValueType,                         ///< elements' type
    std::size_t Dimensions,                        ///< number of dimensions
    array_order Order   = array_order::ROW_MAJOR,  ///< storage order
    bool        IsConst = false
>
class iterator
{
public:
    using this_type         = iterator<ValueType, Dimensions, Order, IsConst>;
    using size_type         = std::size_t;
    using view_type         = internal::conditional_t<IsConst,
                                  const view<ValueType, Dimensions, Order, true>,
                                        view<ValueType, Dimensions, Order, false>>;
    using hyper_index_type  = index<Dimensions>;
    using flat_index_type   = std::ptrdiff_t;

    // iterator traits
    // http://en.cppreference.com/w/cpp/iterator/iterator_traits
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = ValueType;
    using pointer           = internal::conditional_t<IsConst,
                                  const value_type*,
                                        value_type*>;
    using reference         = internal::conditional_t<IsConst,
                                  const value_type&,
                                        value_type&>;
    using difference_type   = std::ptrdiff_t;

private:
    // since an iterator is associated with a view, _cursor is going to be a "relative" index
    // the view is going to handle the relative<->absolute translation
    // therefore _end == hyper range
    // and the cursor_distance_to_origin == cursor (because it's relative to the view's _begin)

    /// views are cheap to create.
    /// also this solves the problem of constructing an iterator from an rvalue view&&
    view_type        _view;
    hyper_index_type _end;     ///< one past the end. "relative" end -- i.e. view.lengths()
    hyper_index_type _cursor;

public:

    // implicit conversion to a const iterator from any (const/non-const) iterators
    operator iterator<value_type, Dimensions, Order, true>() const
    {
        return {_view, _cursor};
    }

    iterator()
    : _view{nullptr}
    , _end{}
    , _cursor{}
    {}

    iterator(const this_type& other)
    : _view{other._view}
    , _end{other._end}
    , _cursor{other._cursor}
    {}

    iterator(this_type&& other)
    : _view{std::move(other._view)}
    , _end{std::move(other._end)}
    , _cursor{std::move(other._cursor)}
    {}

    iterator(const view_type& view_)  // const typename std::remove_const<view_type>::type ?
    : _view{view_}
    , _end(view_.lengths())
    , _cursor{0}
    {}

    iterator(const view_type& view_,
             const flat_index_type cursor)
    : _view{view_}
    , _end(view_.lengths())
    , _cursor{0}
    { advance_cursor(cursor); }

    iterator(const view_type& view_,
             const hyper_index_type& cursor)
    : _view{view_}
    , _end(view_.lengths())
    , _cursor{cursor}
    {}

    // template parameters
    static constexpr size_type   dimensions() { return Dimensions; }
    static constexpr array_order order()      { return Order;      }

    const hyper_index_type& cursor()                  const noexcept { return _cursor; }
          difference_type   cursor(const size_type i) const          { assert(i < Dimensions); return _cursor[i]; }
    const hyper_index_type& end()                     const noexcept { return _end; }
          difference_type   end(const size_type i)    const          { assert(i < Dimensions); return _end[i];    }

    // RandomAccessIterator interface
    // http://www.cplusplus.com/reference/iterator/RandomAccessIterator/
    // https://allthingscomputation.wordpress.com/2013/10/02/an-example-of-a-random-access-iterator-in-c11/
    // http://zotu.blogspot.fr/2010/01/creating-random-access-iterator.html
    // dereferencing
    reference operator*()  const { return _view.operator[](_cursor);                 }
    pointer   operator->() const { return std::addressof(_view.operator[](_cursor)); }
    // prefix inc/dec
    this_type& operator++() { return advance_cursor( 1); }
    this_type& operator--() { return advance_cursor(-1); }
    // postfix inc/dec
    this_type operator++(const int) { this_type prev{*this}; advance_cursor( 1); return prev; }
    this_type operator--(const int) { this_type prev{*this}; advance_cursor(-1); return prev; }
    // compound assignment
    this_type& operator+=(const difference_type d) { return advance_cursor( d); }
    this_type& operator-=(const difference_type d) { return advance_cursor(-d); }
    // arithmetic operations with integral types: "iterator +/- number" AND "number +/- iterator"
    this_type operator+(const difference_type& d) const { return (this_type{*this}).advance_cursor( d); }
    this_type operator-(const difference_type& d) const { return (this_type{*this}).advance_cursor(-d); }
    friend this_type operator+(const difference_type& d, const this_type& it) { return it + d; }
    friend this_type operator-(const difference_type& d, const this_type& it) { return it - d; }
    // arithmetic operations with other iterators
    difference_type operator-(const this_type& other) const
    {
        return flat_cursor() - other.flat_cursor();
    }
    // assignment
    this_type& operator=(const this_type& other)
    {
        _view   = other._view;
        _cursor = other._cursor;
        _end    = other._end;

        return *this;
    }
    this_type& operator=(this_type&& other)
    {
        _view   = other._view;
        _cursor = std::move(other._cursor);
        _end    = std::move(other._end);

        return *this;
    }
    // comparison
    bool operator==(const this_type& other) const { return _cursor == other._cursor; }
    bool operator!=(const this_type& other) const { return _cursor != other._cursor; }
    bool operator< (const this_type& other) const { return _cursor <  other._cursor; }
    bool operator<=(const this_type& other) const { return _cursor <= other._cursor; }
    bool operator> (const this_type& other) const { return _cursor >  other._cursor; }
    bool operator>=(const this_type& other) const { return _cursor >= other._cursor; }
    // swap
    void swap(this_type& other)
    {
        this_type tmp{std::move(other)};
        other = std::move(*this);
        this->operator=(std::move(tmp));
    }
    friend void swap(this_type& lhs, this_type& rhs)
    {
        lhs.swap(rhs);
    }

private:

    // increments/decrements the cursor by the given flattened distance_
    this_type& advance_cursor(difference_type distance_)
    {
        if (_cursor >= _end)
        {
            if (distance_ < 0)
            {
                // _end is "1 past the end"
                // the usual use of it in "arithmetic expressions" is something like
                //   "end - 1": the last accessible element
                //   "end - n": the n'th element starting from the end
                // if cursor is >= _end, "cursor - 1" is NOT the last accessible element
                // therefore, _cursor and distance_ have to be re-adjusted
                _cursor = _end - 1;
                ++distance_;  // yes, increment -- i.e. reduce the distance (distance < 0)
            }
            else
            {
                // the cursor is out of bounds, nothing else to do
                return *this;
            }
        }

        const difference_type distance_to_origin = flat_cursor() + distance_;

        if (distance_to_origin >= static_cast<difference_type>(_view.size()))
        {
            _cursor = _end;
        }
        else if (distance_to_origin <= 0)
        {
            _cursor = hyper_index_type{0};
        }
        else
        {
            internal::advance_cursor_dispatch(array_order_tag<Order>{},
                                              _cursor,
                                              distance_to_origin,
                                              hyper_index_type{0},
                                              _end);
        }

        return *this;
    }

    difference_type flat_cursor() const
    {
        return internal::cursor_distance_to_origin<Order>(_cursor, _end);
    }

};

/// A multi-dimensional view
template <
    typename    ValueType,                          ///< elements' type
    std::size_t Dimensions,                         ///< number of dimensions
    array_order Order   = array_order::ROW_MAJOR,   ///< storage order
    bool        IsConst = false                     ///< same idea as iterator
>
class view
{
public:
    // <editor-fold defaultstate="collapsed" desc="STL-like types">
    // from <array>
    using value_type             = ValueType;
    using pointer                = internal::conditional_t<IsConst, const value_type*, value_type*>;
    using const_pointer          = const value_type*;
    using reference              = internal::conditional_t<IsConst, const value_type&, value_type&>;
    using const_reference        = const value_type&;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    using iterator               = hyper_array::iterator<value_type, Dimensions, Order, IsConst>;
    using const_iterator         = hyper_array::iterator<value_type, Dimensions, Order, true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // others
    using array_type             = internal::conditional_t<IsConst,
                                                           const hyper_array::array<value_type, Dimensions, Order>,
                                                                 hyper_array::array<value_type, Dimensions, Order>>;
    using view_type              = hyper_array::view<value_type, Dimensions, Order, IsConst>;
    using flat_index_type        = std::size_t;
    using hyper_index_type       = hyper_array::index<Dimensions>;
    // </editor-fold>

private:
    // @todo change _flatRange to _size
    // @todo remove either _end or _lengths
    array_type*                         _array;
    hyper_index_type                    _begin;
    //hyper_index_type                    _end;         ///< "one past the end"
    ::std::array<size_type, Dimensions> _lengths;
    size_type                           _size;

public:

    // implicit conversion to a const view from any (const/non-const) view
    // this is a workaround for creating const_iterator from non-const views
    operator view<value_type, Dimensions, Order, true>() const
    {
        return {*_array, _begin, _begin + _lengths};
    }

    view() = delete;

    view(const view_type& other)
    : _array{other._array}
    , _begin{other._begin}
    //, _end{other._end}
    , _lengths(other._lengths)
    , _size{other._size}
    {}// assert(_begin < _end); }

    view(view_type&& other)
    : _array{other._array}
    , _begin{std::move(other._begin)}
    //, _end{std::move(other._end)}
    , _lengths(std::move(other._lengths))
    , _size{other._size}
    {}// assert(_begin < _end); }

    view(array_type& array_)
    : _array(std::addressof(array_))
    , _begin{0}
    //, _end(array_.lengths())
    , _lengths(array_.lengths())
    , _size{array_.size()}
    {}// assert(_begin < _end); }

    view(array_type& array_,
         const hyper_index_type& begin_,
         const hyper_index_type& end_)
    : _array(std::addressof(array_))
    , _begin{begin_}
    //, _end{end_}
    , _lengths([this](const hyper_index_type& range){
        decltype(_lengths) result;
        std::copy(range.begin(), range.end(), result.begin());
        return result;
    }(end_ - begin_))
    , _size{internal::compute_flat_range<Dimensions>(begin_, end_)}
    {}// assert(_begin < _end); }

    view(array_type& array_,
         const hyper_index_type&                    begin_,
         const ::std::array<size_type, Dimensions>& lengths_)
    : _array(std::addressof(array_))
    , _begin{begin_}
    //, _end{begin_ + lengths_}
    , _lengths(lengths_)
    , _size{std::accumulate(lengths_.begin(), lengths_.end(),
                            static_cast<size_type>(1), std::multiplies<size_type>{})}
    {}// assert(_begin < _end); }

    /// copy the contents of @p other 's hyper range into `this` 's hyper range
    /// @note @p other can be any view as long as `other.size() == this->size()`
    ///       this allows for "reshaping" data
    /// Usage example:
    /// @code
    ///     array<int, 3, array_order::ROW_MAJOR> a{2, 4, 3};
    ///     // init a...
    ///     array<double, 2, array_order::COLUMN_MAJOR> b{3, 2};
    ///     view<int, a.dimensions(), a.order()> va{a, {1, 1, 0}, {2, 3, 3}};
    ///     view<double, b.dimensions(), b.order()> vb{b};
    ///     vb = va;
    /// @endcode
    template <
        typename    T_ValueType,
        std::size_t V_Dimensions,
        array_order V_Order,
        bool        V_IsConst
    >
    view_type& operator=(const view<T_ValueType, V_Dimensions, V_Order, V_IsConst>& other)
    {
        assert(other.size() == size());          // allow "reshaping": versatile
        //assert(other.lengths() == lengths());  // require exact match

        std::copy(other.begin(), other.end(), begin());

        return *this;
    }

    // <editor-fold defaultstate="collapsed" desc="Whole-Array Iterators">
    // from <array>
          iterator         begin()         noexcept { return iterator(*this);                  }
    const_iterator         begin()   const noexcept { return const_iterator(*this);            }
          iterator         end()           noexcept { return iterator(*this, lengths());       }
    const_iterator         end()     const noexcept { return const_iterator(*this, lengths()); }
          reverse_iterator rbegin()        noexcept { return reverse_iterator(end());          }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end());    }
          reverse_iterator rend()          noexcept { return reverse_iterator(begin());        }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin());  }
    const_iterator         cbegin()  const noexcept { return const_iterator(*this);            }
    const_iterator         cend()    const noexcept { return const_iterator(*this, lengths()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end());    }
    const_reverse_iterator crend()   const noexcept { return const_reverse_iterator(begin());  }
    // </editor-fold>

    //hyper_index_type begin_index() const noexcept { return _begin; }
    //hyper_index_type end_index()   const noexcept { return _end; }

    // hyper array interface

    // <editor-fold defaultstate="collapsed" desc="Template Arguments">
    /// number of dimensions
    static constexpr size_type   dimensions() noexcept { return Dimensions; }
    /// the convention used for arranging the elements
    static constexpr array_order order()      noexcept { return Order;      }
    // </editor-fold>


    size_type length(const size_type dimensionIndex) const
    {
        assert(dimensionIndex < Dimensions);

        return _lengths[dimensionIndex];
    }

    const ::std::array<size_type, Dimensions>& lengths() const noexcept
    {
        return _lengths;
    }

//    size_type coeff(const size_type coeffIndex) const
//    {
//        assert(coeffIndex < Dimensions);
//
//        return _array->coeffs()[coeffIndex];
//    }

//    const ::std::array<size_type, Dimensions>& coeffs() const noexcept
//    {
//        return _array->coeffs();
//    }

    size_type size() const noexcept
    {
        return _size;
    }

    //      pointer data()       noexcept { return _array->data(); }
    //const_pointer data() const noexcept { return _array->data(); }
    pointer data() const noexcept { return _array->data(); }

    //      reference operator[](const flat_index_type  idx)       { return _array->operator[](absolute_index(idx)); }
    //const_reference operator[](const flat_index_type  idx) const { return _array->operator[](absolute_index(idx)); }
    reference operator[](const flat_index_type  idx) const { return _array->operator[](absolute_index(idx)); }
    //      reference operator[](const hyper_index_type idx)       { return _array->operator[](absolute_index(idx)); }
    //const_reference operator[](const hyper_index_type idx) const { return _array->operator[](absolute_index(idx)); }
    reference operator[](const hyper_index_type idx) const { return _array->operator[](absolute_index(idx)); }

    //template <typename... Indices>
    //internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
    //                      value_type&>
    //at(Indices... indices)
    //{
    //    hyper_index_type idx{indices...};
    //    return validateIndexRanges(idx)
    //         ? operator[](idx)
    //         : operator[](_size);
    //}
    //
    //template <typename... Indices>
    //internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
    //                      const value_type&>
    //at(Indices... indices) const
    //{
    //    hyper_index_type idx{indices...};
    //    return validateIndexRanges(idx)
    //         ? operator[](idx)
    //         : operator[](_size);
    //}

    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          reference>
    at(Indices... indices) const
    {
        const hyper_index_type idx{indices...};
        return validateIndexRanges(idx)
             ? operator[](idx)
             : operator[](_size);
    }

    //template <typename... Indices>
    //internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
    //                      value_type&>
    //operator()(Indices... indices)        { return _array->operator[](hyper_index_type{indices...}); }
    //
    //template <typename... Indices>
    //internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
    //                      const value_type&>
    //operator()(Indices... indices) const { return _array->operator[](hyper_index_type{indices...}); }

    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          reference>
    operator()(Indices... indices) const { return operator[](hyper_index_type{indices...}); }

    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          flat_index_type>
    flatIndex(Indices... indices) const
    {
        return internal::cursor_distance_to_origin(
            ((index<Dimensions>{indices...}) - _begin)._indices,
            _lengths);
    }


private:

    void advance_cursor(hyper_index_type& cursor, difference_type distance_) const
    {
        if ((_begin + cursor) >= _lengths)
        {
            if (distance_ < 0)
            {
                // _end is "1 past the end"
                // the usual use of it in "arithmetic expressions" is something like
                //   "end - 1": the last accessible element
                //   "end - n": the n'th element starting from the end
                // if cursor is >= _end, "cursor - 1" is NOT the last accessible element
                // therefore, _cursor and distance_ have to be re-adjusted
                cursor = (_begin + _lengths) - 1;
                ++distance_;  // yes, increment -- i.e. reduce the distance (distance < 0)
            }
            else
            {
                return;
            }
        }

        const difference_type distance_to_origin =
            internal::cursor_distance_to_origin<Order>((cursor - _begin)._indices,
                                                       _lengths)
            + distance_;

        if (distance_to_origin >= static_cast<difference_type>(_size))
        {
            cursor = _begin + _lengths;
        }
        else if (distance_to_origin <= 0)
        {
            cursor = _begin;
        }
        else
        {
            internal::advance_cursor_dispatch(array_order_tag<Order>{},
                                              cursor,
                                              distance_to_origin,
                                              _begin,
                                              _begin + _lengths);
        }
    }

    bool validateIndexRanges(const hyper_index_type& idx) const
    {
        // check all indices and prepare an exhaustive report (in oss)
        // if some of them are out of bounds
        std::ostringstream oss;
        for (flat_index_type i = 0; i < Dimensions; ++i)
        {
            if ((idx[i] >= (_begin[i] + _lengths[i])) || (idx[i] < 0))
            {
                oss << "Index #" << i << " [== " << idx[i] << "]"
                    << " is out of the [0, " << ((_begin[i] + _lengths[i])-1) << "] range. ";
            }
        }

        // if nothing has been written to oss then all indices are valid
        assert(oss.str().empty());
        return oss.str().empty();
        //if (oss.str().empty())
        //{
        //    return true;
        //}
        //else
        //{
        //    throw std::out_of_range(oss.str());
        //}
    }

    hyper_index_type absolute_index(const flat_index_type relative) const
    {
        hyper_index_type absolute{_begin};
        advance_cursor(absolute, static_cast<difference_type>(relative));
        return absolute;
    }

    hyper_index_type absolute_index(const hyper_index_type& relative) const
    {
        return _begin + relative;
    }

};

/// A multi-dimensional array
/// Inspired by [orca_array](https://github.com/astrobiology/orca_array)
template <
    typename    ValueType,                      ///< elements' type
    std::size_t Dimensions,                     ///< number of dimensions
    array_order Order = array_order::ROW_MAJOR  ///< storage order
>
class array
{
    // Types ///////////////////////////////////////////////////////////////////////////////////////

public:

    // <editor-fold defaultstate="collapsed" desc="STL-like types">
    // from <array>
    using value_type             = ValueType;
    using pointer                = value_type*;
    using const_pointer          = const value_type*;
    using reference              = value_type&;
    using const_reference        = const value_type&;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // others
    using array_type             = array<value_type, Dimensions, Order>;
    using flat_index_type        = std::size_t;
    using hyper_index_type       = index<Dimensions>;
    // </editor-fold>

    // Attributes //////////////////////////////////////////////////////////////////////////////////

    // <editor-fold desc="Class Attributes">
private:
    // ::std::array's are used here mainly (only?) because they are initializable
    // from `std::initializer_list` and they support move semantics
    // cf. hyper_array::array's constructors
    // also ::std::array seem to introduce no overhead over the data they hold
    // i.e. sizeof(::std::array<Type, Length>) == sizeof(Type) * Length

    /// number of elements in each dimension
    ::std::array<size_type, Dimensions> _lengths;

    /// coefficients to use when computing the index
    /// @see at()
    ::std::array<size_type, Dimensions> _coeffs;

    /// total number of elements in the data array
    size_type _size;

    /// handles the lifecycle of the dynamically allocated data array
    /// The user doesn't need to access it directly
    /// If the user needs access to the allocated array, they can use data()
    std::unique_ptr<value_type[]> _dataOwner;
    // </editor-fold>

    // methods /////////////////////////////////////////////////////////////////////////////////////

public:

    // <editor-fold defaultstate="collapsed" desc="Constructors">
    /// would it make sense to create an array without specifying the dimension lengths ?
    array() = delete;

    /// copy-constructor
    array(const array_type& other)
    : _lengths   (other._lengths)
    , _coeffs    (other._coeffs)
    , _size      (other._size)
    , _dataOwner {other.cloneData()}
    {}

    /// move constructor
    array(array_type&& other)
    : _lengths   (std::move(other._lengths))
    , _coeffs    (std::move(other._coeffs))
    , _size      (other._size)
    , _dataOwner {std::move(other._dataOwner)}
    {}

    /// the usual way of constructing hyper arrays
    template <
        typename... DimensionLengths,
        typename = internal::enable_if_t<
            (sizeof...(DimensionLengths) == Dimensions) && internal::are_integral<DimensionLengths...>::value,
            void>
    >
    array(DimensionLengths... dimensionLengths)
    : _lengths   {{static_cast<size_type>(dimensionLengths)...}}
    , _coeffs    (internal::computeIndexCoeffs<size_type, Dimensions, Order>(_lengths))
    , _size      (computeDataSize(_lengths))
    , _dataOwner {allocateData(_size)}
    {}

    /// Creates a new hyper array from "raw data"
    ///
    /// @note `*this` will maintain ownership of `rawData`
    ///       unless e.g. data are `std::move`d from it
    array(::std::array<size_type, Dimensions> lengths,  ///< length of each dimension
          value_type* rawData = nullptr  ///< raw data
                                         ///< must contain `computeIndexCoeffs(lengths)`
                                         ///< if `nullptr`, a new data array will be allocated
    )
    : _lengths   (std::move(lengths))
    , _coeffs    (internal::computeIndexCoeffs<size_type, Dimensions, Order>(lengths))
    , _size      (computeDataSize(_lengths))
    , _dataOwner {rawData == nullptr ? allocateData(_size).release() : rawData}
    {}

    /// Creates a new hyper array from an initializer list
    array(::std::array<size_type, Dimensions> lengths,  ///< length of each dimension
          std::initializer_list<value_type>   values,   ///< {the initializer list}
          const value_type& defaultValue      = {}      ///< default value, in case `values.size() < size()`
    )
    : _lengths   (std::move(lengths))
    , _coeffs    (internal::computeIndexCoeffs<size_type, Dimensions, Order>(lengths))
    , _size      (computeDataSize(_lengths))
    , _dataOwner {allocateData(_size).release()}
    {
        assert(values.size() <= size());

        std::copy(values.begin(),
                  values.end(),
                  _dataOwner.get());

        // fill any remaining number of uninitialized elements with the default value
        if (values.size() < size())
        {
            std::fill(_dataOwner.get() + values.size(),
                      _dataOwner.get() + size(),
                      defaultValue);
        }
    }
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="Assignment Operators">
    /// copy assignment
    array_type& operator=(const array_type& other)
    {
        _lengths   = other._lengths;
        _coeffs    = other._coeffs;
        _size      = other._size;
        _dataOwner = other.cloneData();

        return *this;
    }

    /// move assignment
    array_type& operator=(array_type&& other)
    {
        _lengths   = std::move(other._lengths);
        _coeffs    = std::move(other._coeffs);
        _size      = other._size;
        _dataOwner = std::move(other._dataOwner);

        return *this;
    }
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="Whole-Array Iterators">
    // from <array>
          iterator         begin()         noexcept { return iterator(data());                }
    const_iterator         begin()   const noexcept { return const_iterator(data());          }
          iterator         end()           noexcept { return iterator(data() + size());       }
    const_iterator         end()     const noexcept { return const_iterator(data() + size()); }
          reverse_iterator rbegin()        noexcept { return reverse_iterator(end());         }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end());   }
          reverse_iterator rend()          noexcept { return reverse_iterator(begin());       }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }
    const_iterator         cbegin()  const noexcept { return const_iterator(data());          }
    const_iterator         cend()    const noexcept { return const_iterator(data() + size()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end());   }
    const_reverse_iterator crend()   const noexcept { return const_reverse_iterator(begin()); }
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="Template Arguments">
    /// number of dimensions
    static constexpr size_type   dimensions() noexcept { return Dimensions; }
    /// the convention used for arranging the elements
    static constexpr array_order order()      noexcept { return Order;      }
    // </editor-fold>

    /// Returns the length of a given dimension at run-time
    size_type length(const size_type dimensionIndex) const
    {
        assert(dimensionIndex < Dimensions);

        return _lengths[dimensionIndex];
    }

    /// Returns a reference to the _lengths array
    const ::std::array<size_type, Dimensions>& lengths() const noexcept
    {
        return _lengths;
    }

    /// Returns the given dimension's coefficient (used for computing the "linear" index)
    size_type coeff(const size_type coeffIndex) const
    {
        assert(coeffIndex < Dimensions);

        return _coeffs[coeffIndex];
    }

    /// Returns a reference to the _coeffs array
    const ::std::array<size_type, Dimensions>& coeffs() const noexcept
    {
        return _coeffs;
    }

    /// Returns the total number of elements in data
    size_type size() const noexcept
    {
        return _size;
    }

    /// Returns a pointer to the allocated data array
    value_type* data() noexcept
    {
        return _dataOwner.get();
    }

    /// `const` version of data()
    const value_type* data() const noexcept
    {
        return _dataOwner.get();
    }

    /// Returns the element at index `idx` in the data array
    value_type& operator[](const flat_index_type idx)
    {
        return _dataOwner[idx];
    }

    /// `const` version of operator[]
    const value_type& operator[](const flat_index_type idx) const
    {
        return _dataOwner[idx];
    }

    /// Returns the element at index `idx` in the data array
    value_type& operator[](const hyper_index_type& idx)
    {
        return _dataOwner[internal::flatten_index(idx._indices, _coeffs)];
    }

    /// `const` version of operator[]
    const value_type& operator[](const hyper_index_type& idx) const
    {
        return _dataOwner[internal::flatten_index(idx._indices, _coeffs)];
    }

    /// Returns the element at the given index tuple
    /// Usage:
    /// @code
    ///     hyper_array::array<double, 3> arr(4, 5, 6);
    ///     arr.at(3, 1, 4) = 3.14;
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          value_type&>
    at(Indices... indices)
    {
        return _dataOwner[flatIndex_checkBounds(indices...)];
    }

    /// `const` version of at()
    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          const value_type&>
    at(Indices... indices) const
    {
        return _dataOwner[flatIndex_checkBounds(indices...)];
    }

    /// Unchecked version of at()
    /// Usage:
    /// @code
    ///     hyper_array::array<double, 3> arr(4, 5, 6);
    ///     arr(3, 1, 4) = 3.14;
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          value_type&>
    operator()(Indices... indices)
    {
        return _dataOwner[
            internal::flatten_index<Dimensions,
                                    size_type,
                                    flat_index_type>({{static_cast<flat_index_type>(indices)...}},
                                                     _coeffs)
        ];
    }

    /// `const` version of operator()
    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          const value_type&>
    operator()(Indices... indices) const
    {
        return _dataOwner[
            internal::flatten_index<Dimensions,
                                    size_type,
                                    flat_index_type>({{static_cast<flat_index_type>(indices)...}},
                                                     _coeffs)
        ];
    }

    /// returns the actual index of the element in the data array
    /// Usage:
    /// @code
    ///     hyper_array::array<int, 3> arr(4, 5, 6);
    ///     assert(&arr.at(3, 1, 4) == &arr.data()[arr.flatIndex(3, 1, 4)]);
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          flat_index_type>
    flatIndex(Indices... indices) const
    {
        return flatIndex_checkBounds(indices...);
    }

private:

    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          ::std::array<flat_index_type, Dimensions>>
    validateIndexRanges(Indices... indices) const
    {
        ::std::array<flat_index_type,
                     Dimensions> indexArray = {{static_cast<flat_index_type>(indices)...}};

        // check all indices and prepare an exhaustive report (in oss)
        // if some of them are out of bounds
        std::ostringstream oss;
        for (flat_index_type i = 0; i < Dimensions; ++i)
        {
            if ((indexArray[i] >= _lengths[i]) || (indexArray[i] < 0))
            {
                oss << "Index #" << i << " [== " << indexArray[i] << "]"
                    << " is out of the [0, " << (_lengths[i]-1) << "] range. ";
            }
        }

        // if nothing has been written to oss then all indices are valid
        assert(oss.str().empty());
        return indexArray;
        //if (oss.str().empty())
        //{
        //    return indexArray;
        //}
        //else
        //{
        //    throw std::out_of_range(oss.str());
        //}
    }

    template <typename... Indices>
    internal::enable_if_t<internal::are_indices<Dimensions, Indices...>::value,
                          flat_index_type>
    flatIndex_checkBounds(Indices... indices) const
    {
        return internal::flatten_index(validateIndexRanges(indices...), _coeffs);
    }

    /// computes the total number of elements in a data array
    static
    constexpr
    size_type
    computeDataSize(const ::std::array<size_type, Dimensions>& dimensionLengths) noexcept
    {
        return internal::ct_accumulate(dimensionLengths,
                                       0,
                                       Dimensions,
                                       static_cast<size_type>(1),
                                       internal::ct_prod<size_type, size_type>);
    }

    static
    std::unique_ptr<value_type[]> allocateData(const size_type elementCount)
    {
        #if (__cplusplus < 201402L)  // C++14 ?
        return std::unique_ptr<value_type[]>{new value_type[elementCount]};
        #else
        // std::make_unique() is not part of C++11
        return std::make_unique<value_type[]>(elementCount);
        #endif

    }

    std::unique_ptr<value_type[]> cloneData() const
    {
        // allocate the new data container
        std::unique_ptr<value_type[]> dataOwner{allocateData(size())};

        // copy data to the the new container
        std::copy(_dataOwner.get(),
                  _dataOwner.get() + size(),
                  dataOwner.get());

        return dataOwner;
    }

};

// <editor-fold desc="orca_array-like declarations">
template<typename ValueType> using array1d = array<ValueType, 1>;
template<typename ValueType> using array2d = array<ValueType, 2>;
template<typename ValueType> using array3d = array<ValueType, 3>;
template<typename ValueType> using array4d = array<ValueType, 4>;
template<typename ValueType> using array5d = array<ValueType, 5>;
template<typename ValueType> using array6d = array<ValueType, 6>;
template<typename ValueType> using array7d = array<ValueType, 7>;
template<typename ValueType> using array8d = array<ValueType, 8>;
template<typename ValueType> using array9d = array<ValueType, 9>;
// </editor-fold>

}

#if HYPER_ARRAY_CONFIG_Overload_Stream_Operator

// pretty printing of a bounds' single dimensional range
//template <size_t Dimensions>
//inline
//std::ostream& operator<<(std::ostream& out,
//                         const typename hyper_array::bounds<Dimensions>::pair& p)
//{
//    out << "[" << p.min << ", " << p.max << "]";
//    return out;
//}

/// pretty printing of array order to the standard library's streams
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::array_order& o)
{
    switch (o)
    {
    case hyper_array::array_order::ROW_MAJOR   : out << "ROW_MAJOR"   ; break;
    case hyper_array::array_order::COLUMN_MAJOR: out << "COLUMN_MAJOR"; break;
    }
    return out;
}

namespace hyper_array
{
namespace internal
{

/// an efficient way for doing:
/// @code
///     for (auto& x : container) {
///         out << x << separator;
///     }
/// @endcode
template <typename ContainerType>
inline
void copyToStream(ContainerType&& container,
                  std::ostream&   out,
                  const char      separator[] = " ")
{
    std::copy(container.begin(),
              container.end(),
              std::ostream_iterator<decltype(*container.begin())>(out, separator));
}

}
}

/// pretty printing of bounds
template <size_t Dimensions>
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::bounds<Dimensions>& bs)
{
    out << "[ ";
// hyper_array::internal::copyToStream(bs, out);
    for (auto&& p : bs)
    {
//        out << p << " ";
        out << "[" << p.min << " " << p.max << "]" << " ";
    }
    out << "]";
    return out;
}

/// (not so) pretty printing of iterator
template <
    typename    ValueType,
    std::size_t Dimensions,
    hyper_array::array_order Order,
    bool IsConst
>
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::iterator<ValueType, Dimensions, Order, IsConst>& it)
{
    out << "[ ";
    for (size_t i = 0; i < Dimensions; ++i)
    {
        out << "[" << it.cursor(i) << ":" << (it.end(i)-1) << "] ";
    }
    out << "]";
    return out;
}

/// pretty printing of index tuple
template <size_t Dimensions>
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::index<Dimensions>& idx)
{
    out << "( "; hyper_array::internal::copyToStream(idx, out); out << ")";
    return out;
}

/// Pretty printing of hyper arrays to the standard library's streams
///
/// Should print something like
/// @code
///     [dimensions: 2 ][order: ROW_MAJOR ][lengths: 3 4 ][coeffs: 4 1 ][size: 12 ][data: 1 2 3 4 5 6 7 8 9 10 11 12 ]
/// @endcode
template <typename ValueType, size_t Dimensions, hyper_array::array_order Order>
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::array<ValueType, Dimensions, Order>& ha)
{
    using hyper_array::internal::copyToStream;

    out << "[dimensions: " << ha.dimensions()                 << " ]";
    out << "[order: "      << ha.order()                      << " ]";
    out << "[lengths: "     ; copyToStream(ha.lengths(), out) ; out << "]";
    out << "[coeffs: "      ; copyToStream(ha.coeffs(), out)  ; out << "]";
    out << "[size: "       << ha.size()                       << " ]";
    out << "[data: "        ; copyToStream(ha, out)           ; out << "]";

    return out;
}
#endif
