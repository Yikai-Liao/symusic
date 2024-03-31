//
// Created by lyk on 2024/3/6.
//
#include "shared_vec.h"
#include "algorithm"
#include "pdqsort.h"
#include <iostream>
#include "symusic/ops.h"


int main() {
    SharedVec<int> vec {1,2,3,4};
    const auto first = &vec[0];
    for (auto i = 5; i < 100; i++) {
        vec.push_back(i);
    }
    vec.insert(vec.begin() + 10, 10, 99999);
    vec.erase(vec.begin() + 12, vec.begin() + 13);
    vec.pop_back();
    auto new_vec = std::move(vec);
    const auto new_first = &new_vec[0];

    const auto copied_vec = new_vec;
    std::cout << "First address unchanged: " << (first == new_first) << std::endl;
    std::cout << "Copy would change address: " << (first != &copied_vec[0]) << std::endl;

    std::sort(new_vec.begin(), new_vec.end(), [](auto & a, auto & b) { return a > b;});
    // show the vector
    for (auto & i: new_vec) {
        std::cout << i << " ";
    }   std::cout << std::endl;
    pdqsort_branchless(new_vec.begin(), new_vec.end());
    for (auto & i: new_vec) {
        std::cout << i << " ";
    }   std::cout << std::endl;

    auto new_vec2 = symusic::ops::filter(new_vec, [](auto & i) { return i > 50; });
    for (auto & i: new_vec2) {
        std::cout << i << " ";
    }   std::cout << std::endl;
}
