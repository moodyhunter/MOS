// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace++.hpp"
#include "mos/types.h"

struct cdtor_test
{
    static inline int global_int = 0;
    int N = global_int++;
    cdtor_test()
    {
        mos::cout << "[constructor] for n = " << N << " at " << this << mos::endl;
    }

    ~cdtor_test()
    {
        mos::cout << "[destructor]  for n = " << N << " at " << this << mos::endl;
    }
};

static cdtor_test cdtor_test1[10];
static cdtor_test cdtor_test2[10];
static cdtor_test cdtor_test3[10];
static cdtor_test cdtor_test4[10];
static cdtor_test cdtor_test5[10];

int main(int argc, char *argv[])
{
    mos::cout << mos::endl;
    mos::cout << "|====================|" << mos::endl;
    mos::cout << "|  Hello, world!     |" << mos::endl;
    mos::cout << "|====================|" << mos::endl;
    mos::cout << mos::endl;
    mos::cout << "This is a C++ program running in MOS." << mos::endl;
    mos::cout << mos::endl;

    for (int i = 0; i < argc; i++)
    {
        mos::cout << "argv[" << i << "] = " << argv[i] << mos::endl;
    }

    mos::mutex m;
    m.lock();

    int *ptr = new int;
    mos::cout << "ptr = " << ptr << mos::endl;
    delete ptr;

    // print the address of the main function
    mos::cout << "main = " << (void *) main << mos::endl;

    // print a char, a signed int, and an unsigned int, a long, an unsigned long, a long long, and an unsigned long long
    mos::cout << "              char = " << 'a' << mos::endl;
    mos::cout << "        signed int = " << -1 << mos::endl;
    mos::cout << "      unsigned int = " << 1u << mos::endl;
    mos::cout << "       signed long = " << -1l << mos::endl;
    mos::cout << "     unsigned long = " << 1ul << mos::endl;
    mos::cout << "  signed long long = " << -1ll << mos::endl;
    mos::cout << "unsigned long long = " << 1ull << mos::endl;

    return 0;
}
