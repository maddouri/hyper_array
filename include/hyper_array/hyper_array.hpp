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
//#include <algorithm>  // during dev. replaced by compile-time equivalents in hyper_array::internal
#include <array>        // std::array for hyper_array::array::dimensionLengths and indexCoeffs
#include <cassert>      // assert
#include <memory>       // unique_ptr for hyper_array::array::_dataOwner
#if HYPER_ARRAY_CONFIG_Overload_Stream_Operator
#include <ostream>      // ostream for the overloaded operator<<()
#endif
#include <sstream>      // stringstream in hyper_array::array::validateIndexRanges()
#include <type_traits>  // template metaprogramming stuff in hyper_array::internal
// </editor-fold>


/// The hyper_array lib's namespace
namespace hyper_array
{

// <editor-fold defaultstate="collapsed" desc="Internal Helper Blocks">
/// Helper functions for hyper_array::array's implementation
/// @note Everything here is subject to change and must NOT be used by user code
namespace internal
{
    /// shorthand for the enable_if syntax
    /// @see http://en.cppreference.com/w/cpp/types/enable_if#Helper_types
    template <bool b, typename T = void>
    using enable_if_t = typename std::enable_if<b, T>::type;

    /// building block for a neat trick for checking multiple types against a given trait
    template <bool...>
    struct bool_pack
    {};
    /// Neat trick for checking multiple types against a given trait
    /// https://codereview.stackexchange.com/a/107903/86688
    template <bool... bs>
    using are_all_true = std::is_same<bool_pack<true, bs...>,
                                      bool_pack<bs..., true>>;

    /// Checks that all the template arguments are integral types
    /// @note `T&` where `std::is_integral<T>::value==true` is considered integral
    /// by removing any reference then using `std::is_integral`
    template <typename... Ts>
    using are_integral = are_all_true<
        std::is_integral<
            typename std::remove_reference<Ts>::type
        >::value...
    >;

    /// Compile-time sum
    template <typename T>
    constexpr T ct_plus(const T x, const T y)
    {
        return x + y;
    }

    /// Compile-time product
    template <typename T>
    constexpr T ct_prod(const T x, const T y)
    {
        return x * y;
    }

    /// Compile-time equivalent to `std::accumulate()`
    template
    <
        typename    T,  ///< result type
        std::size_t N,  ///< length of the array
        typename    O   ///< type of the binary operation
    >
    constexpr
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

    /// Compile-time equivalent to `std::inner_product()`
    template
    <
        typename T,      ///< the result type
        typename T_1,    ///< first array's type
        size_t   N_1,    ///< length of the first array
        typename T_2,    ///< second array's type
        size_t   N_2,    ///< length of the second array
        typename O_SUM,  ///< summation operation's type
        typename O_PROD  ///< multiplication operation's type
    >
    constexpr
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
}
// </editor-fold>

/// A multi-dimensional array
/// Inspired by [orca_array](https://github.com/astrobiology/orca_array)
template
<
    typename    ValueType,  ///< elements' type
    std::size_t Dimensions  ///< number of dimensions
>
class array
{
    // Types ///////////////////////////////////////////////////////////////////////////////////////

public:

    // <editor-fold defaultstate="collapsed" desc="STL-like types">
    // from <array>
    using value_type             = ValueType;
    // using pointer                = value_type*;
    // using const_pointer          = const value_type*;
    // using reference              = value_type&;
    // using const_reference        = const value_type&;
    using iterator               = value_type*;
    using const_iterator         = const value_type*;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // others
    using array_type             = array<value_type, Dimensions>;
    using index_type             = std::size_t;
    // </editor-fold>

    // Attributes //////////////////////////////////////////////////////////////////////////////////

    // <editor-fold desc="Static Attributes">
public:
    /// number of dimensions
    static constexpr size_type dimensions = Dimensions;
    // </editor-fold>

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
    ///
    /* doc-style comment block disabled becaus doxygen/doxypress can't handle it
       just copy/pase into : https://www.codecogs.com/latex/eqneditor.php
        \f[
            \begin{cases}
            C_i = \begin{cases}
                  \prod_{j=i+1}^{n-2} L_j  &  \text{if } i \in [0, n-2]  \\
                  1                        &  \text{if } i = n-1
            \end{cases}
            \\
            \begin{cases}
                n   &: \text{Dimensions - 1}  \\
                C_i &: \text{\_coeffs[i]}     \\
                L_j &: \text{\_lengths[j]}
            \end{cases}
            \end{cases}
        \f]
    */
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
    template
    <
        typename... DimensionLengths,
        typename = internal::enable_if_t<
            sizeof...(DimensionLengths) == Dimensions
            && internal::are_integral<DimensionLengths...>::value>
    >
    array(DimensionLengths... dimensionLengths)
    : _lengths   {{static_cast<size_type>(dimensionLengths)...}}
    , _coeffs    (computeIndexCoeffs(_lengths))
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
    , _coeffs    (computeIndexCoeffs(lengths))
    , _size      (computeDataSize(_lengths))
    , _dataOwner {rawData == nullptr ? allocateData(_size).release() : rawData}
    {}
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

    /// Returns the length of a given dimension at run-time
    size_type length(const size_type dimensionIndex) const
    {
        assert(dimensionIndex < Dimensions);
        //if (dimensionIndex >= Dimensions)
        //{
        //    throw std::out_of_range("The dimension index must be within [0, Dimensions-1]");
        //}

        return _lengths[dimensionIndex];
    }

    /// Returns a reference to the _lengths array
    const decltype(_lengths)&
    lengths() const noexcept
    {
        return _lengths;
    }

    /// Returns the given dimension's coefficient (used for computing the "linear" index)
    size_type coeff(const size_type coeffIndex) const
    {
        assert(coeffIndex < Dimensions);
        //if (coeffIndex >= Dimensions)
        //{
        //    throw std::out_of_range("The coefficient index must be within [0, Dimensions-1]");
        //}

        return _coeffs[coeffIndex];
    }

    /// Returns a reference to the _coeffs array
    const decltype(_coeffs)&
    coeffs() const noexcept
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
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
                          value_type&>
    at(Indices... indices)
    {
        return _dataOwner[rawIndex(indices...)];
    }

    /// `const` version of at()
    template <typename... Indices>
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
                          const value_type&>
    at(Indices... indices) const
    {
        return _dataOwner[rawIndex(indices...)];
    }

    /// Unchecked version of at()
    /// Usage:
    /// @code
    ///     hyper_array::array<double, 3> arr(4, 5, 6);
    ///     arr(3, 1, 4) = 3.14;
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
                          value_type&>
    operator()(Indices... indices)
    {
        return _dataOwner[rawIndex_noChecks({{static_cast<index_type>(indices)...}})];
    }

    /// `const` version of operator()
    template <typename... Indices>
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
                          const value_type&>
    operator()(Indices... indices) const
    {
        return _dataOwner[rawIndex_noChecks({{static_cast<index_type>(indices)...}})];
    }

    /// Returns the actual index of the element in the data array
    /// Usage:
    /// @code
    ///     hyper_array::array<int, 3> arr(4, 5, 6);
    ///     assert(&arr.at(3, 1, 4) == &arr.data()[arr.rawIndex(3, 1, 4)]);
    /// @endcode
    template <typename... Indices>
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
                          index_type>
    rawIndex(Indices... indices) const
    {
        return rawIndex_noChecks(validateIndexRanges(indices...));
    }

private:

    template <typename... Indices>
    internal::enable_if_t<sizeof...(Indices) == Dimensions
                          && internal::are_integral<Indices...>::value,
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

    constexpr
    index_type
    rawIndex_noChecks(const ::std::array<index_type, Dimensions>& indexArray) const noexcept
    {
        /* https://www.codecogs.com/latex/eqneditor.php
           \begin{cases}
           I_{actual} &= \sum_{i=0}^{N-1} {C_i \cdot I_i}                  \\
                                                                           \\
           I_{actual} &: \text{actual index of the data in the data array} \\
           N          &: \text{Dimensions}                                 \\
           C_i        &: \text{\_coeffs[i]}                                \\
           I_i        &: \text{indexArray[i]}                              \\
           \end{cases}
        */
        return internal::ct_inner_product(_coeffs, 0,
                                          indexArray, 0,
                                          Dimensions,
                                          static_cast<index_type>(0),
                                          internal::ct_plus<index_type>,
                                          internal::ct_prod<index_type>);
    }

    static
    ::std::array<size_type, Dimensions>
    computeIndexCoeffs(const ::std::array<size_type, Dimensions>& dimensionLengths) noexcept
    {
        ::std::array<size_type, Dimensions> coeffs;
        coeffs[Dimensions - 1] = 1;
        for (size_type i = 0; i < (Dimensions - 1); ++i)
        {
            coeffs[i] = internal::ct_accumulate(dimensionLengths,
                                                i + 1,
                                                Dimensions - i - 1,
                                                static_cast<size_type>(1),
                                                internal::ct_prod<size_type>);
        }
        return coeffs;
    }

    /// computes the total number of elements in a data array
    static
    constexpr
    size_type
    computeDataSize(const ::std::array<size_type,
                                       Dimensions>& lengths  ///< lengths of each dimension
                   ) noexcept
    {
        return internal::ct_accumulate(lengths,
                                       0,
                                       Dimensions,
                                       static_cast<size_type>(1),
                                       internal::ct_prod<size_type>);
    }

    static
    std::unique_ptr<value_type[]> allocateData(const size_type dataSize)
    {
        #if (__cplusplus < 201402L)  // C++14 ?
        return std::unique_ptr<value_type[]>{new value_type[dataSize]};
        #else
        // std::make_unique() is not part of C++11
        return std::make_unique<value_type[]>(dataSize);
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

        //
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
/// Pretty printing to the standard library's streams
///
/// Should print something like
/// @code
///     [dimensions: 2 ][lengths: 3 4 ][coeffs: 4 1 ][size: 12 ][data: 1 2 3 4 5 6 7 8 9 10 11 12 ]
/// @endcode
template <typename T, size_t D>
std::ostream& operator<<(std::ostream& out, const hyper_array::array<T, D>& ha)
{
    out << "[dimensions: " << ha.dimensions << " ]";

    out << "[lengths: ";
    for (auto& l : ha.lengths()) { out << l << " "; }
    out << "]";

    out << "[coeffs: ";
    for (auto& c : ha.coeffs()) { out << c << " "; }
    out << "]";

    out << "[size: " << ha.size() << " ]";

    out << "[data: ";
    for (auto& x : ha) { out << x << " "; }
    out << "]";

    return out;
}
#endif
