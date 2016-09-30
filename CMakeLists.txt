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
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O1 -march=native -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O3 -march=native -DNDEBUG -fno-exceptions")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr")
elseif(UNIX)
  set(CMAKE_CXX_FLAGS_DEBUG "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O1 -pthread -fPIC -march=native  -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -O3 -pthread -fPIC -march=native -DNDEBUG -DGTEST_USE_OWN_TR1_TUPLE=0 -fno-exceptions")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined")
endif()

# Will download gtest and make it part of the source
include(cmake_extensions/DownloadProject.cmake)
download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

download_project(PROJ                gflags
                 GIT_REPOSITORY      https://github.com/gflags/gflags.git
                 GIT_TAG             master
                 ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)
add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})

# When using CMake 2.8.11 or later, header path dependencies
# are automatically added to the gtest and gmock targets.
# For earlier CMake versions, we have to explicitly add the
# required directories to the header search path ourselves.
include_directories("${gtest_SOURCE_DIR}/include"
                     "${gmock_SOURCE_DIR}/include")

# Will need protobufs and pcap
find_package(Protobuf REQUIRED)
find_package(PCAP REQUIRED)
find_package(Ctemplate REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS} ${PCAP_INCLUDE_DIR} ${CMAKE_BINARY_DIR} ${CTEMPLATE_INCLUDE_DIR})

# Creates C resources file from files in given directory
function(create_resources dir output prefix)
    # Create empty output file
    file(WRITE ${output} "")
    # Collect input files
    file(GLOB bins ${dir}/*)
    # Iterate through input files
    foreach(bin ${bins})
        # Get short filename
        string(REGEX MATCH "([^/]+)$" filename ${bin})
        # Replace filename spaces & extension separator for C compatibility
        string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
        # Read hex data from file
        file(READ ${bin} filedata HEX)
        # Convert hex data for C compatibility
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
        # Append data to output file
        file(APPEND ${output} "const unsigned char ${prefix}_${filename}[] = {${filedata}};\nconst unsigned ${prefix}_${filename}_size = sizeof(${prefix}_${filename});\n")
    endforeach()
endfunction()

create_resources("${PROJECT_SOURCE_DIR}/data/www" "${PROJECT_BINARY_DIR}/www_resources.c" "www")
create_resources("${PROJECT_SOURCE_DIR}/data/grapher_templates" "${PROJECT_BINARY_DIR}/grapher_resources.c" "grapher")

macro(add_test_exec name src_file deps)
  add_executable(${name} ${src_file})
  target_link_libraries(${name} gmock_main ${deps} ${ARGN})
  add_test(NAME ${name} COMMAND ${name})
endmacro(add_test_exec)  

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

################################
# Web
################################
set(WEB_HEADER_FILES src/web/web_page.h src/web/mongoose/mongoose.h src/web/graph.h)
add_library(ncode_web STATIC src/web/web_page.cc src/web/mongoose/mongoose.c src/web/graph.cc ${PROJECT_BINARY_DIR}/www_resources.c ${WEB_HEADER_FILES})
target_link_libraries(ncode_web ${PROTOBUF_LIBRARIES} ncode_common ncode_net ${CTEMPLATE_LIBRARIES})

add_test_exec(web_page_test src/web/web_page_test.cc ncode_web)
add_test_exec(graph_test src/web/graph_test.cc ncode_web)

################################
# Metrics
################################
PROTOBUF_GENERATE_CPP(PROTO_METRICS_SRCS PROTO_METRICS_HDRS src/metrics/metrics.proto)
set_property(SOURCE ${PROTO_METRICS_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-extended-offsetof")
set(METRICS_HEADER_FILES src/metrics/metrics.h src/metrics/metrics_parser.h ${PROTO_METRICS_HDRS})
add_library(ncode_metrics STATIC src/metrics/metrics.cc src/metrics/metrics_parser.cc ${PROTO_METRICS_SRCS} ${METRICS_HEADER_FILES})
target_link_libraries(ncode_metrics ${PROTOBUF_LIBRARIES} ncode_common ncode_web gflags)

add_library(metrics_test_util STATIC src/metrics/metrics_test_util.cc)
add_test_exec(metrics_test src/metrics/metrics_test.cc ncode_metrics metrics_test_util)
add_test_exec(metrics_parser_test src/metrics/metrics_parser_test.cc ncode_metrics metrics_test_util)

################################
# Grapher
################################
set(METRICS_HEADER_FILES src/grapher/grapher.h)
add_library(ncode_grapher STATIC src/grapher/grapher.cc ${PROJECT_BINARY_DIR}/grapher_resources.c)
target_link_libraries(ncode_grapher ncode_common ncode_web)

add_test_exec(grapher_test src/grapher/grapher_test.cc ncode_grapher)
