//
// Created by HWZ on 2023/12/5.
//
#include "CFAHBenchLib.h"

#include <iostream>

void Message(void*, const char *str){
    std::cout << str << std::endl;
}

int main()try{
    auto simulation = CreateSimulation(1000 * 10);

    PrepareSimulation(simulation, nullptr, Message, nullptr, nullptr);

    auto res = RunSimulation(simulation, nullptr, nullptr, nullptr, nullptr);

    std::cout << res << std::endl;

    DestroySimulation(simulation);
    return 0;
}
catch(std::exception &e){
    std::cout << e.what() << std::endl;
    return 1;
}