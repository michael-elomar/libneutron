cmake_minimum_required(VERSION 3.5)
project(neutron LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES
	src/loop.c
	src/ctx.c
	src/conn.c
	src/evt.c
	src/timer.c
)

set(INCLUDE
	include
	src
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE})
# set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/neutron.h)

#======== Install Support ========#

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include)

# Install the headers
install(DIRECTORY include/ DESTINATION include)
