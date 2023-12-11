
add_library(CFAHBench SHARED
    CFAHBenchLib.cpp
    CFAHBenchLib.h
)

target_compile_definitions(CFAHBench PRIVATE
    -DCFAHBENCH_EXPORTS
)

target_link_libraries(CFAHBench fahbench)

add_executable(test test.cpp)

target_link_libraries(test CFAHBench)
