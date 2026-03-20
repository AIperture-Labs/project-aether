#include "Obj.hpp"

#include <iostream>
#include <string>

#include "config.hpp"

PROJECT_API void obj_test()
{
    std::string test = "v 1.000 -1.223 -0.334";
    std::string tag  = "Hello";
    std::string tag2 = "Hello";

    std::cout << "Result: " << (test == "v") << std::endl;
    std::cout << "Result: " << (tag == tag2) << std::endl;
    std::cout << "Result: " << test.compare("v") << std::endl;
    std::cout << "Result: " << test.compare("t") << std::endl;
}