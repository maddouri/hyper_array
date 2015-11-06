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
//#include <algorithm>       // during dev. replaced by compile-time equivalents in hyper_array::internal
#include <array>             // std::array for hyper_array::array::dimensionLengths and indexCoeffs
#include <cassert>           // assert()
#include <cstddef>           // ptrdiff_t
#include <initializer_list>  // std::initializer_list for the constructors
#include <memory>            // std::unique_ptr for hyper_array::array::_dataOwner
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

// <editor-fold defaultstate="collapsed" desc="Internal Helper Blocks">
/// helper functions for hyper_array::array's implementation
/// @note Everything here is subject to change and must NOT be used by user code
namespace internal
{

/// shorthand for the enable_if syntax
/// @see http://en.cppreference.com/w/cpp/types/enable_if#Helper_types
template <bool b, typename T>
using enable_if_t = typename std::enable_if<b, T>::type;

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

/// compile-time sum
template <typename T>
inline constexpr T ct_plus(const T x, const T y) { return x + y; }

/// compile-time product
template <typename T>
inline constexpr T ct_prod(const T x, const T y) { return x * y; }

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
                   const size_t  first_1,        ///< from this position
                   const ::std::array<T_2, N_2>& arr_2,  ///< with this array
                   const size_t  first_2,        ///< from this position
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
                                            ct_prod<size_type>);
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
                                            ct_prod<size_type>);
    }
    return coeffs;
}

/// computes the "flat" or "linear" or "1D" index from the provided index tuple @p indexArray
/// and the index coefficients @p coeffs
template <std::size_t Dimensions, typename size_type, typename index_type>
inline constexpr
size_type
flatten_index(const ::std::array<index_type, Dimensions>& indexArray,
              const ::std::array<size_type, Dimensions>&  coeffs) noexcept
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
                                      static_cast<index_type>(0),
                                      internal::ct_plus<index_type>,
                                      internal::ct_prod<index_type>);
}

template <std::size_t Dimensions, typename Iterable>
ptrdiff_t compute_flat_range(const Iterable& begin_, const Iterable& end_)
{
    // compute the range of each dimension
    ::std::array<ptrdiff_t, Dimensions> ranges;
    std::transform(end_.begin(), end_.end(),
                   begin_.begin(),
                   ranges.begin(),
                   std::minus<ptrdiff_t>());
    // the "flat range" is the product of the sub ranges
    return std::accumulate(ranges.begin(), ranges.end(),
                           static_cast<ptrdiff_t>(1),
                           std::multiplies<ptrdiff_t>());
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
    using reference       = ptrdiff_t&;
    using const_reference = const ptrdiff_t&;
    using size_type       = std::size_t;
    using value_type      = ptrdiff_t;

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
    : _indices([](){
        ::std::array<value_type, Dimensions> indices;
        indices.fill(0);
        return indices;
    }())
    {}

    index(const index<Dimensions>& other)
    : _indices(other._indices)
    {}

    index(index<Dimensions>&& other)
    : _indices(std::move(other._indices))
    {}

//    index(std::initializer_list<value_type> indices)
//    : _indices([&indices](){
//        assert(indices.size() == Dimensions);
//        ::std::array<value_type, Dimensions> idxs;
//        std::copy(indices.begin(), indices.end(), idxs.begin());
//        return idxs;
//    }())
//    {}

    template <
    typename... Indices,
    typename = internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
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

    index<Dimensions>& operator=(const index<Dimensions>& other)
    {
        std::copy(other._indices.begin(), other._indices.end(), _indices.begin());
        return *this;
    }

    bool operator==(const index<Dimensions>& other) const { return _indices == other._indices; }
    bool operator!=(const index<Dimensions>& other) const { return _indices != other._indices; }
    bool operator< (const index<Dimensions>& other) const { return _indices <  other._indices; }
    bool operator<=(const index<Dimensions>& other) const { return _indices <= other._indices; }
    bool operator> (const index<Dimensions>& other) const { return _indices >  other._indices; }
    bool operator>=(const index<Dimensions>& other) const { return _indices >= other._indices; }
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
    using index_type             = std::size_t;
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
    value_type& operator[](const index_type idx)
    {
        return _dataOwner[idx];
    }

    /// `const` version of operator[]
    const value_type& operator[](const index_type idx) const
    {
        return _dataOwner[idx];
    }

    /// Returns the element at the given index tuple
    /// Usage:
    /// @code
    ///     hyper_array::array<double, 3> arr(4, 5, 6);
    ///     arr.at(3, 1, 4) = 3.14;
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        value_type&>
    at(Indices... indices)
    {
        return _dataOwner[rawIndex_checkBounds(indices...)];
    }

    /// `const` version of at()
    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        const value_type&>
    at(Indices... indices) const
    {
        return _dataOwner[rawIndex_checkBounds(indices...)];
    }

    /// Unchecked version of at()
    /// Usage:
    /// @code
    ///     hyper_array::array<double, 3> arr(4, 5, 6);
    ///     arr(3, 1, 4) = 3.14;
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        value_type&>
    operator()(Indices... indices)
    {
        return _dataOwner[internal::flatten_index<Dimensions,
                                                  size_type,
                                                  index_type>({{static_cast<index_type>(indices)...}},
                                                              _coeffs)];
    }

    /// `const` version of operator()
    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        const value_type&>
    operator()(Indices... indices) const
    {
        return _dataOwner[internal::flatten_index<Dimensions,
                                                  size_type,
                                                  index_type>({{static_cast<index_type>(indices)...}},
                                                              _coeffs)];
    }

    /// returns the actual index of the element in the data array
    /// Usage:
    /// @code
    ///     hyper_array::array<int, 3> arr(4, 5, 6);
    ///     assert(&arr.at(3, 1, 4) == &arr.data()[arr.rawIndex(3, 1, 4)]);
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        index_type>
    rawIndex(Indices... indices) const
    {
        return rawIndex_checkBounds(indices...);
    }

private:

    template <typename... Indices>
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        ::std::array<index_type, Dimensions>>
    validateIndexRanges(Indices... indices) const
    {
        ::std::array<index_type, Dimensions> indexArray = {{static_cast<index_type>(indices)...}};

        // check all indices and prepare an exhaustive report (in oss)
        // if some of them are out of bounds
        std::ostringstream oss;
        for (index_type i = 0; i < Dimensions; ++i)
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
    internal::enable_if_t<
        (sizeof...(Indices) == Dimensions) && internal::are_integral<Indices...>::value,
        index_type>
    rawIndex_checkBounds(Indices... indices) const
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
                                       internal::ct_prod<size_type>);
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

/// Iterates over the elements of a hyper array that are comprised within the limits
/// defined by a `bounds<Dimensions>`
template <
    typename    ValueType,                      ///< elements' type
    std::size_t Dimensions,                     ///< number of dimensions
    array_order Order = array_order::ROW_MAJOR  ///< storage order
>
class iterator
{
public:
    using this_type       = iterator<ValueType, Dimensions, Order>;
    using size_type       = std::size_t;
    using value_type      = ValueType;
    using index_type      = ptrdiff_t;
    using difference_type = ptrdiff_t;

private:
    template <array_order O> struct array_order_tag {};

public:

    array<ValueType, Dimensions, Order>* _array;

    index<Dimensions> _begin;
    index<Dimensions> _cursor;
    index<Dimensions> _end;
    ptrdiff_t         _flatRange;

public:

    iterator()
    : _array{nullptr}
    , _begin{}
    , _cursor{}
    , _end{}
    , _flatRange{}
    {}

    iterator(const this_type& other)
    : _array{other._array}
    , _begin{other._begin}
    , _cursor{other._cursor}
    , _end{other._end}
    , _flatRange{other._flatRange}
    {}

    iterator(const index<Dimensions>& begin_,
             const index<Dimensions>& end_)
    : _array{nullptr}
    , _begin{begin_}
    , _cursor{_begin}
    , _end{end_}
    , _flatRange{internal::compute_flat_range<Dimensions>(_begin, _end)}
    {}

    // RandomAccessIterator interface
    // dereferencing
    ValueType& operator*()  const { return (*_array)[internal::flatten_index(_cursor._indices, _array->coeffs())]; }
    ValueType& operator->() const { return (*_array)[internal::flatten_index(_cursor._indices, _array->coeffs())]; }
    // prefix
    this_type& operator++() { advance_cursor( 1); return *this; }
    this_type& operator--() { advance_cursor(-1); return *this; }
    // postfix
    this_type operator++(const int) { auto prev = *this; advance_cursor( 1); return prev; }
    this_type operator--(const int) { auto prev = *this; advance_cursor(-1); return prev; }
    // assignment
    this_type& operator=(const this_type& other)
    {
        _array     = other._array;
        _begin     = other._begin;
        _cursor    = other._cursor;
        _end       = other._end;
        _flatRange = other._flatRange;
    }
    // comparison
    // @todo a better implementation
    bool operator==(const this_type& other) const { return _cursor == other._cursor; }
    bool operator!=(const this_type& other) const { return _cursor != other._cursor; }
    bool operator< (const this_type& other) const { return _cursor <  other._cursor; }
    bool operator<=(const this_type& other) const { return _cursor <= other._cursor; }
    bool operator> (const this_type& other) const { return _cursor >  other._cursor; }
    bool operator>=(const this_type& other) const { return _cursor >= other._cursor; }

    void advance_cursor(const difference_type distance_)
    {
        const ptrdiff_t distance_to_origin = cursor_distance_to_origin() + distance_;
        advance_cursor_dispatch(array_order_tag<Order>{}, distance_to_origin);
    }

private:

    ptrdiff_t cursor_distance_to_origin() const
    {
        return cursor_distance_to_origin_dispatch(array_order_tag<Order>{});
    }

    ptrdiff_t cursor_distance_to_origin_dispatch(const array_order_tag<array_order::COLUMN_MAJOR>&) const
    {
        index<Dimensions> diff;
        // diff = last - first
        std::transform(_cursor.begin(), _cursor.end(),
                       _begin.begin(),
                       diff.begin(),
                       std::minus<ptrdiff_t>());
        ::std::array<ptrdiff_t, Dimensions> ranges;
        std::transform(_end.begin(), _end.end(),
                       _begin.begin(),
                       ranges.begin(),
                       std::minus<ptrdiff_t>());
        ptrdiff_t d = 0;
        d += diff[0];
        for (ptrdiff_t i = 1; i < Dimensions; ++i)
        {
            d += diff[i] * std::accumulate(ranges.begin(), ranges.begin() + i,
                                           static_cast<ptrdiff_t>(1),
                                           std::multiplies<ptrdiff_t>());
        }
        return d;
    }

    ptrdiff_t cursor_distance_to_origin_dispatch(const array_order_tag<array_order::ROW_MAJOR>&) const
    {
        index<Dimensions> diff;
        // diff = last - first
        std::transform(_cursor.begin(), _cursor.end(),
                       _begin.begin(),
                       diff.begin(),
                       std::minus<ptrdiff_t>());
        ::std::array<ptrdiff_t, Dimensions> ranges;
        std::transform(_end.begin(), _end.end(),
                       _begin.begin(),
                       ranges.begin(),
                       std::minus<ptrdiff_t>());
        ptrdiff_t d = 0;
        d += diff[Dimensions - 1];
        for (ptrdiff_t i = 1; i < Dimensions; ++i)
        {
            d += diff[Dimensions - 1 - i] * std::accumulate(ranges.rbegin(), ranges.rbegin() + i,
                                                            static_cast<ptrdiff_t>(1),
                                                            std::multiplies<ptrdiff_t>());
        }
        return d;
    }

    void advance_cursor_dispatch(const array_order_tag<array_order::COLUMN_MAJOR>&,
                                 const difference_type distance_to_origin)
    {
        if (distance_to_origin >= _flatRange)
        {
            _cursor = _end;
            return;
        }

        // basic algorithm: here for reference
//        // range = _end - _begin
//        ::std::array<ptrdiff_t, Dimensions> range;
//        std::transform(_end.begin(), _end.end(),
//                       _begin.begin(),
//                       range.begin(),
//                       std::minus<ptrdiff_t>());
//
//        // compute the offset of each "component" (the offset is the remainder)
//        ::std::array<ptrdiff_t, Dimensions> r;  // remainder
//        ::std::array<ptrdiff_t, Dimensions> q;  // quotient
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
//                       std::plus<ptrdiff_t>());

        // implementation 2: simplification/refactoring of the "basic algorithm"
        ptrdiff_t range;
        ptrdiff_t q = distance_to_origin;

        _cursor = _begin;
        for (ptrdiff_t i = 0; i < Dimensions; ++i)
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

    void advance_cursor_dispatch(const array_order_tag<array_order::ROW_MAJOR>&,
                                 const difference_type distance_to_origin)
    {
        if (distance_to_origin >= _flatRange)
        {
            _cursor = _end;
            return;
        }

        // implementation 2: simplification/refactoring of the "basic algorithm"
        ptrdiff_t range;
        ptrdiff_t q = distance_to_origin;

        _cursor = _begin;
        for (ptrdiff_t i = Dimensions - 1; i >= 0; --i)
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
    hyper_array::array_order Order
>
inline
std::ostream& operator<<(std::ostream& out,
                         const hyper_array::iterator<ValueType, Dimensions, Order>& it)
{
    out << "[ ";
    for (size_t i = 0; i < Dimensions; ++i)
    {
        out << "[" << it._begin[i] << " " << it._cursor[i] << " " << it._end[i] << "] ";
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
