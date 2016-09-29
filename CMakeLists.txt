cmake_minimum_required(VERSION 2.8.7)

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
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O1 -march=native -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O3 -march=native -DNDEBUG -fno-exceptions")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")
elseif(UNIX)
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O1 -pthread -fPIC -march=native  -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Werror -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O3 -pthread -fPIC -march=native -DNDEBUG -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined")
endif()

include(cmake_extensions/DownloadProject.cmake)
download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

# When using CMake 2.8.11 or later, header path dependencies
# are automatically added to the gtest and gmock targets.
# For earlier CMake versions, we have to explicitly add the
# required directories to the header search path ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include"
                        "${gmock_SOURCE_DIR}/include")
endif()

# Will need protobufs and pcap
find_package(Protobuf REQUIRED)
find_package(PCAP REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS} ${PCAP_INCLUDE_DIR} ${CMAKE_BINARY_DIR})

function(add_test_exec name src_file deps)
  add_executable(${name} ${src_file})
  target_link_libraries(${name} gmock_main ${deps})
  add_test(NAME ${name} COMMAND ${name})
endfunction(add_test_exec)  

################################
# Common stuff
################################
set(COMMON_HEADER_FILES src/common/common.h src/common/substitute.h src/common/logging.h src/common/file.h src/common/stringpiece.h src/common/strutil.h src/common/map_util.h src/common/stl_util.h src/common/event_queue.h src/common/free_list.h src/common/packer.h src/common/ptr_queue.h)
add_library(ncode_common STATIC src/common/common.cc src/common/substitute.cc src/common/logging.cc src/common/file.cc src/common/stringpiece.cc src/common/strutil.cc src/common/event_queue.cc src/common/free_list.cc src/common/packer.cc ${COMMON_HEADER_FILES})

set_property(SOURCE src/common/stringpiece_test.cc APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-conversion-null -Wno-sign-compare")
set_property(SOURCE src/common/strutil_test.cc APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-sign-compare")
add_test_exec(common_stringpiece_test src/common/stringpiece_test.cc ncode_common)
add_test_exec(common_logging_test src/common/logging_test.cc ncode_common)
add_test_exec(common_test src/common/common_test.cc ncode_common)
add_test_exec(common_strutil_test src/common/strutil_test.cc ncode_common)
add_test_exec(common_event_queue_test src/common/event_queue_test.cc ncode_common)
add_test_exec(common_free_list_test src/common/free_list_test.cc ncode_common)
add_test_exec(common_packer_test src/common/packer_test.cc ncode_common)
add_test_exec(common_ptr_queue_test src/common/ptr_queue_test.cc ncode_common)
add_test_exec(common_circular_array_test src/common/circular_array_test.cc ncode_common)

################################
# Network-releated stuff
################################
PROTOBUF_GENERATE_CPP(PROTO_NET_SRCS PROTO_NET_HDRS src/net/net.proto)
set_property(SOURCE ${PROTO_NET_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-extended-offsetof")
set(NET_HEADER_FILES src/net/net_common.h src/net/net_gen.h src/net/pcap.h ${PROTO_NET_HDRS})
add_library(ncode_net STATIC src/net/net_common.cc src/net/net_gen.cc src/net/pcap.cc ${PROTO_NET_SRCS} ${NET_HEADER_FILES})
target_link_libraries(ncode_net ${PCAP_LIBRARY} ${PROTOBUF_LIBRARIES} ncode_common)

add_test_exec(net_common_test src/net/net_common_test.cc ncode_net)
add_test_exec(net_gen_test src/net/net_gen_test.cc ncode_net)
