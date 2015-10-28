
// clang++-3.7 -stdlib=libc++ -std=c++11 -Wall -Wextra -Wpedantic -Weverything -Wno-c++98-compat -Werror ${file} -lc++ -lc++abi -o -o ${file_path}/${file_base_name}
// g++ -std=c++11 -std=c++11 -fdiagnostics-show-option -Wall -Wextra -Wpedantic -Werror ${file} -o ${file_path}/${file_base_name}

// std
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <vector>
// hyper_array
#include "../include/hyper_array/hyper_array.hpp"

using std::cout;
using std::endl;

// shorthand for prints a hyper_array
#define printarr(arr) cout << #arr << ": " << arr << endl;

int main()
{
    // size
    {
        cout << "\nsize\n";

        using el_type = double;
        constexpr size_t elementCount = 10;

        constexpr size_t dataSize = elementCount * sizeof(el_type);

        constexpr size_t std_array_overhead   = sizeof(std::array<el_type, elementCount>) - dataSize;
        constexpr size_t hyper_array_overhead = sizeof(hyper_array::array1d<el_type>);
        constexpr size_t std_vector_overhead  = sizeof(std::vector<el_type>(elementCount));

        cout << "std::array  overhead: " << std_array_overhead   << " bytes" << endl;
        cout << "hyper_array overhead: " << hyper_array_overhead << " bytes" << endl;
        cout << "std::vector overhead: " << std_vector_overhead  << " bytes" << endl;

    }

    // 3d array
    {
        cout << "\n3d array\n";

        hyper_array::array3d<double> aa{2, 3, 4};

        int c = 0;
        for (auto&& x : aa)
        {
            x = - c++;
        }

        printarr(aa)
    }

    // construction, moving, assignment
    {
        cout << "\nconstruction, moving, assignment\n";

        constexpr size_t elementCount = 3;
        using ha_type = hyper_array::array1d<double>;
        ha_type aa{hyper_array::array1d<double>{elementCount}};
        ha_type bb{aa.length(0)};
        ha_type cc(2);

        for(typename ha_type::index_type i = 0; i < elementCount; ++i)
        {
            aa[i] = static_cast<double>(elementCount * i);
        }

        printarr(aa)

        bb = std::move(aa);
        cc = bb;
        bb[0] = -3;

        printarr(bb)
        printarr(cc)

        const ha_type dd(cc);
        printarr(dd)
    }

    // algorithms
    {
        cout << "\nalgorithms\n";

        constexpr size_t dimensions = 3;
        using el_type = double;
        using ha_type = hyper_array::array<el_type, dimensions>;
        const ::std::array<size_t, dimensions> lengths{{2,3,4}};

        ha_type aa{lengths};
        printarr(aa)  // uninitialized
        std::iota(aa.begin(), aa.end(), 1);
        printarr(aa)

        ha_type bb{aa.lengths()};
        std::copy(aa.begin(), aa.end(), bb.rbegin());
        printarr(bb)

        ha_type cc{aa.lengths()};
        std::transform(aa.begin(), aa.end(),
                       bb.begin(),
                       cc.begin(),
                       [](el_type a, el_type b) {
                           return a + b;
                       });
        printarr(cc);
    }

    // in containers
    {
        cout << "\nin containers\n";

        using el_type = double;
        constexpr size_t dims = 2;
        using ha_type = hyper_array::array<el_type, dims>;
        std::vector<ha_type> vv;

        vv.emplace_back(hyper_array::array<double, dims>{1, 2});
        vv.push_back(hyper_array::array2d<double>{3, 4});
        vv.push_back(ha_type{5, 6});
        vv.push_back({7, 8});
        vv.emplace_back(9, 10);

        for (auto&& ha : vv)
        {
            std::iota(ha.begin(), ha.end(), 1);
            cout << "vv[" << std::distance(&vv[0], &ha) << "] ";
            printarr(ha)
        }
    }


    cout << "\ndone" << endl;
}
