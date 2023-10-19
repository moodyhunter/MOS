// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <vector>

int main()
{
    std::cout << "Hello, world, from hosted C++ runtime library!" << std::endl;
    std::vector<int> v = { 1, 2, 3 };
    for (const auto &i : v)
        std::cout << i << std::endl;
    return 0;
}
