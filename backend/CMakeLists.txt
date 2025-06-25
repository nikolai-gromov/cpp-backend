cmake_minimum_required(VERSION 3.9)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

# Defining the game_model library
add_library(game_model STATIC
	src/util/geom.h
	src/util/tagged.h
	src/generator/loot_generator.h
    src/generator/loot_generator.cpp
	src/detector/collision_detector.h
	src/detector/collision_detector.cpp
	src/model/model.h
    src/model/model.cpp
)

target_link_libraries(game_model PRIVATE CONAN_PKG::boost)

# Definition of tests
add_executable(game_server_tests
	src/util/extra_data.h
	src/app/app.h
	src/app/app.cpp
	tests/model_tests.cpp
    tests/loot_generator_tests.cpp
	tests/collision-detector-tests.cpp
	src/loader/json_loader.h
	src/loader/json_loader.cpp
	tests/state-serialization-tests.cpp
)

target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 game_model CONAN_PKG::boost)

# Defining the Primary Server
add_executable(game_server
	src/util/extra_data.h
	src/util/boost_json.cpp
	src/loader/json_loader.h
	src/loader/json_loader.cpp
	src/app/app.h
	src/app/app.cpp
	src/server/http_server.h
	src/server/http_server.cpp
	src/util/common.h
	src/util/common.cpp
	src/handler/api_handler.h
	src/handler/api_handler.cpp
	src/handler/request_handler.h
	src/handler/request_handler.cpp
	src/serialization/model_serialization.h
	src/main.cpp
)

target_link_libraries(game_server PRIVATE game_model CONAN_PKG::boost Threads::Threads)

# Commands for connecting sanitizers
# target_compile_options(game_server PRIVATE -fsanitize=thread)
# target_link_libraries(game_server PRIVATE -fsanitize=thread)