cmake_minimum_required(VERSION 2.8.11)

project(NCode)
include(CTest)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_extensions)

if (CMAKE_VERSION VERSION_LESS 3.2)
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 1")
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   message(STATUS "Setting build type to 'Debug' as none was specified.")
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
   # Set the possible values of build type for cmake-gui
   set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -Wno-sign-compare -O1 -march=native -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -Wno-sign-compare -O3 -march=native -DNDEBUG -fno-exceptions")
elseif(UNIX)
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Wuninitialized -Wall -Wextra -Wno-sign-compare -O1 -pthread -fPIC -march=native  -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Wuninitialized -Wall -Wextra -Wno-sign-compare -O3 -pthread -fPIC -march=native -DNDEBUG -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions")
endif()

set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")

include(cmake_extensions/DownloadProject.cmake)
download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

# Will need protobufs and pcap
find_package(Protobuf REQUIRED)
find_package(PCAP REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS} ${PCAP_INCLUDE_DIR})

################################
# Common stuff
################################
set(COMMON_HEADER_FILES src/common/common.h src/common/substitute.h src/common/logging.h src/common/file.h src/common/stringpiece.h src/common/strutil.h src/common/map_util.h src/common/stl_util.h src/common/event_queue.h src/common/free_list.h src/common/packer.h src/common/ptr_queue.h)
add_library(ncode_common STATIC src/common/common.cc src/common/substitute.cc src/common/logging.cc src/common/file.cc src/common/stringpiece.cc src/common/strutil.cc src/common/event_queue.cc src/common/free_list.cc src/common/packer.cc ${COMMON_HEADER_FILES})

set_source_files_properties(src/common/stringpiece_test.cc PROPERTIES COMPILE_FLAGS -Wno-conversion-null)
add_executable(common_stringpiece_test src/common/stringpiece_test.cc)
target_link_libraries(common_stringpiece_test gtest_main ncode_common)
add_test(NAME common_stringpiece_test COMMAND common_stringpiece_test)

add_executable(common_logging_test src/common/logging_test.cc)
target_link_libraries(common_logging_test gmock_main ncode_common)
add_test(NAME common_logging_test COMMAND common_logging_test)

add_executable(common_test src/common/common_test.cc)
target_link_libraries(common_test gmock_main ncode_common)
add_test(NAME common_test COMMAND common_test)

add_executable(common_strutil_test src/common/strutil_test.cc)
target_link_libraries(common_strutil_test gmock_main ncode_common)
add_test(NAME common_strutil_test COMMAND common_strutil_test)

add_executable(common_event_queue_test src/common/event_queue_test.cc)
target_link_libraries(common_event_queue_test gmock_main ncode_common)
add_test(NAME common_event_queue_test COMMAND common_event_queue_test)

add_executable(common_free_list_test src/common/free_list_test.cc)
target_link_libraries(common_free_list_test gmock_main ncode_common)
add_test(NAME common_free_list_test COMMAND common_free_list_test)

add_executable(common_packer_test src/common/packer_test.cc)
target_link_libraries(common_packer_test gmock_main ncode_common)
add_test(NAME common_packer_test COMMAND common_packer_test)

add_executable(common_ptr_queue_test src/common/ptr_queue_test.cc)
target_link_libraries(common_ptr_queue_test gmock_main ncode_common)
add_test(NAME common_ptr_queue_test COMMAND common_ptr_queue_test)

add_executable(common_circular_array_test src/common/circular_array_test.cc)
target_link_libraries(common_circular_array_test gmock_main ncode_common)
add_test(NAME common_circular_array_test COMMAND common_circular_array_test)
