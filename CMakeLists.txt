cmake_minimum_required(VERSION 3.5)
project(neutron LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUT_DIR ${CMAKE_SOURCE_DIR}/out/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/bin/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/lib/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}/lib/)

set(SRC_FILES
	src/loop.c
	src/ctx.c
	src/evt.c
	src/timer.c
)

set(INCLUDE
	include
	src
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})

add_executable(test_loop examples/test_loop.c)
target_link_libraries(test_loop ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})

add_executable(test_loop_cpp examples/test_loop.cpp)
target_link_libraries(test_loop_cpp ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})

add_executable(test_ctx examples/test_ctx.c)
target_link_libraries(test_ctx ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})

add_executable(test_ctx_udp examples/test_ctx_udp.c)
target_link_libraries(test_ctx_udp ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})
