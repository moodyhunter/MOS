// SPDX-License-Identifier: GPL-3.0-or-later

#include "test.pb.h"

#include <mutex>

int main(int, const char *[])
{
    tutorial::Person person;
    person.set_id(123);
    person.set_name("John Doe");
    person.set_email("a@b.c");

    std::string data;
    person.SerializeToString(&data);

    std::cout << "data size: " << data.size() << std::endl;
    std::cout << "data: '" << data << "'" << std::endl;

    tutorial::Person person2;
    person2.ParseFromString(data);
    std::cout << "Deserialized:" << std::endl;
    std::cout << person2.id() << std::endl;
    std::cout << person2.name() << std::endl;
    std::cout << person2.email() << std::endl;

    return 0;
}
