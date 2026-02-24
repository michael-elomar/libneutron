add_executable(example_loop loop.c)
target_link_libraries(example_loop ${PROJECT_NAME})
target_include_directories(example_loop PUBLIC $<BUILD_INTERFACE:${INCLUDE}>)

add_executable(example_loop_cpp loop.cpp)
target_link_libraries(example_loop_cpp ${PROJECT_NAME})
target_include_directories(example_loop_cpp PUBLIC $<BUILD_INTERFACE:${INCLUDE}>)

add_executable(ping_ctx ping_ctx.c)
target_link_libraries(ping_ctx ${PROJECT_NAME})
target_include_directories(ping_ctx PUBLIC $<BUILD_INTERFACE:${INCLUDE}>)

add_executable(ping_ctx_cpp ping_ctx.cpp)
target_link_libraries(ping_ctx_cpp ${PROJECT_NAME})
target_include_directories(ping_ctx_cpp PUBLIC $<BUILD_INTERFACE:${INCLUDE}>)

add_executable(timer timer.c)
target_link_libraries(timer ${PROJECT_NAME})
target_include_directories(timer PUBLIC $<BUILD_INTERFACE:${INCLUDE}>)
