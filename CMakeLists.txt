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

set(ASAN FALSE CACHE BOOL "Compile with ASAN on")
set(TSAN FALSE CACHE BOOL "Compile with TSAN on")

set(BASE_FLAGS "-g -std=c++11 -pedantic-errors -Winit-self -Woverloaded-virtual -Wuninitialized -Wall -Wextra -fno-exceptions")
set(BASE_LD_FLAGS "")
if (ASAN)
   set(BASE_FLAGS "${BASE_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
   set(BASE_LD_FLAGS "${BASE_LD_FLAGS} -fsanitize=address")
endif()
if (TSAN)
   set(BASE_FLAGS "${BASE_FLAGS} -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls")
   set(BASE_LD_FLAGS "${BASE_LD_FLAGS} -fsanitize=thread")
endif()

if(APPLE)
elseif(UNIX)
  set(BASE_FLAGS "${BASE_FLAGS} -pthread")
endif()

set(BASE_DEBUG_FLAGS "${BASE_FLAGS} -O0 -fno-omit-frame-pointer")
set(BASE_OPT_FLAGS "${BASE_FLAGS} -O3 -march=native -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${BASE_DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${BASE_OPT_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} ${BASE_LD_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_RELEASE} ${BASE_LD_FLAGS}")

add_subdirectory(external/backward-cpp)
add_subdirectory(external/googletest)
SET(OLD_BUILD_TESTING ${BUILD_TESTING})
SET(BUILD_TESTING False)
add_subdirectory(external/gflags)
SET(BUILD_TESTING ${OLD_BUILD_TESTING})

# Will need protobufs, pcap and ctemplate
set(PROTOBUF_IMPORT_DIRS ${PROJECT_SOURCE_DIR}/src/net ${PROJECT_SOURCE_DIR}/src/dfs)
find_package(Protobuf REQUIRED)
find_package(PCAP REQUIRED)
find_package(Ctemplate REQUIRED)

# Need a linear optimizer. Will look for both GLPK and CPLEX.
find_package(GLPK)
find_package(CPLEX)

# Check which optimization package is found
set(FORCE_GLPK false CACHE BOOL "If GLPK and CPLEX are found, by default we build against CPLEX. Setting this to true forces to build against GLPK")
if(CPLEX_FOUND)
    if(GLPK_FOUND AND FORCE_GLPK)
        set(OPTIMIZER_INCLUDE_DIRS ${GLPK_INCLUDE_DIR})
        set(OPTIMIZER_LIBRARIES ${GLPK_LIBRARY})
	set(OPTIMIZER "GLPK")
    else()
        set(OPTIMIZER_INCLUDE_DIRS ${CPLEX_INCLUDE_DIRS})
	if(APPLE)
	    set(OPTIMIZER_LIBRARIES -Wl,-force_load ${CPLEX_LIBRARIES})
	elseif(UNIX)
	    set(OPTIMIZER_LIBRARIES -Wl,-whole-archive ${CPLEX_LIBRARIES} -Wl,-no-whole-archive)
	endif()
	   set(OPTIMIZER "CPLEX")
    endif()
else()
    if(GLPK_FOUND)
        set(OPTIMIZER_INCLUDE_DIRS ${GLPK_INCLUDE_DIR})
        set(OPTIMIZER_LIBRARIES ${GLPK_LIBRARY})
	set(OPTIMIZER "GLPK")
    else()
        message(FATAL_ERROR "No LP optimizer found at all!")
    endif()
endif()

include_directories(${PROTOBUF_INCLUDE_DIRS} ${PCAP_INCLUDE_DIR} ${CMAKE_BINARY_DIR} ${CTEMPLATE_INCLUDE_DIR} ${OPTIMIZER_INCLUDE_DIRS})

configure_file(
  "${PROJECT_SOURCE_DIR}/src/config.h.in"
  "${PROJECT_BINARY_DIR}/ncode_config.h")

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
  add_executable(${name} ${src_file} ${BACKWARD_ENABLE})
  target_link_libraries(${name} gtest gmock_main ${deps} ${ARGN})
  add_test(NAME ${name} COMMAND ${name})
  add_backward(${name})
endmacro(add_test_exec)  

################################
# Common stuff
################################
set(COMMON_HEADER_FILES src/common/common.h src/common/substitute.h src/common/logging.h src/common/file.h src/common/stringpiece.h src/common/strutil.h src/common/map_util.h src/common/stl_util.h src/common/event_queue.h src/common/free_list.h src/common/packer.h src/common/ptr_queue.h src/common/lru_cache.h src/common/perfect_hash.h)
add_library(ncode_common STATIC src/common/common.cc src/common/substitute.cc src/common/logging.cc src/common/file.cc src/common/stringpiece.cc src/common/strutil.cc src/common/event_queue.cc src/common/free_list.cc src/common/packer.cc ${COMMON_HEADER_FILES})
add_backward(ncode_common)

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
add_test_exec(common_lru_cache_test src/common/lru_cache_test.cc ncode_common)
add_test_exec(common_thread_runner_test src/common/thread_runner_test.cc ncode_common)
add_test_exec(common_perfect_hash_test src/common/perfect_hash_test.cc ncode_common)

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
# DFS
################################
PROTOBUF_GENERATE_CPP(PROTO_DFS_SRCS PROTO_DFS_HDRS src/dfs/dfs.proto)
set_property(SOURCE ${PROTO_DFS_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-extended-offsetof")
set(DFS_HEADER_FILES src/dfs/constraint.h src/dfs/array_graph.h src/dfs/path_utils.h src/dfs/dfs.h src/dfs/dfs_session.h ${PROTO_DFS_HDRS})
add_library(ncode_dfs STATIC src/dfs/constraint.cc src/dfs/array_graph.cc src/dfs/path_utils.cc src/dfs/dfs.cc src/dfs/dfs_session.cc ${PROTO_DFS_SRCS} ${DFS_HEADER_FILES} ${NET_HEADER_FILES})
target_link_libraries(ncode_dfs ${PROTOBUF_LIBRARIES} ncode_common ncode_net)

add_library(dfs_test_util STATIC src/dfs/test_fixtures.cc)
target_link_libraries(dfs_test_util ncode_dfs gtest)

add_test_exec(dfs_constraint_test src/dfs/constraint_test.cc ncode_dfs dfs_test_util)
add_test_exec(dfs_array_graph_test src/dfs/array_graph_test.cc ncode_dfs dfs_test_util)
add_test_exec(dfs_test src/dfs/dfs_test.cc ncode_dfs dfs_test_util)
add_test_exec(dfs_path_utils_test src/dfs/path_utils_test.cc ncode_dfs dfs_test_util)
add_test_exec(dfs_session_test src/dfs/dfs_session_test.cc ncode_dfs dfs_test_util)

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
target_link_libraries(metrics_test_util ncode_metrics gtest)

add_test_exec(metrics_test src/metrics/metrics_test.cc ncode_metrics metrics_test_util)
add_test_exec(metrics_parser_test src/metrics/metrics_parser_test.cc ncode_metrics metrics_test_util)

################################
# Grapher
################################
set(METRICS_HEADER_FILES src/grapher/grapher.h)
add_library(ncode_grapher STATIC src/grapher/grapher.cc ${PROJECT_BINARY_DIR}/grapher_resources.c)
target_link_libraries(ncode_grapher ncode_common ncode_web)

add_test_exec(grapher_test src/grapher/grapher_test.cc ncode_grapher)

################################
# Linear programming
################################
set(METRICS_HEADER_FILES src/lp/lp.h src/lp/mc_flow.h)
add_library(ncode_lp STATIC src/lp/lp.cc src/lp/mc_flow.cc)
target_link_libraries(ncode_lp ncode_net ${OPTIMIZER_LIBRARIES})

add_test_exec(lp_test src/lp/lp_test.cc ncode_lp)
add_test_exec(lp_mc_flow_test src/lp/mc_flow_test.cc ncode_lp)

################################
# HTSim
################################
set(HTSIM_HEADER_FILES src/htsim/packet.h src/htsim/queue.h src/htsim/match.h src/htsim/udp.h src/htsim/tcp.h src/htsim/network.h src/htsim/flow_driver.h src/htsim/pcap_consumer.h src/htsim/htsim.h src/htsim/bulk_gen.h src/htsim/animator.h)
add_library(ncode_htsim STATIC src/htsim/packet.cc src/htsim/queue.cc src/htsim/match.cc src/htsim/udp.cc src/htsim/tcp.cc src/htsim/network.cc src/htsim/flow_driver.cc src/htsim/pcap_consumer.cc src/htsim/bulk_gen.cc src/htsim/animator.cc)
target_link_libraries(ncode_htsim ncode_net ncode_metrics)

# Test .pcap file needed by the pcap_consumer test
file(COPY data/pcap_test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test_exec(htsim_match_test src/htsim/match_test.cc ncode_htsim)
add_test_exec(htsim_packet_test src/htsim/packet_test.cc ncode_htsim)
add_test_exec(htsim_network_test src/htsim/network_test.cc ncode_htsim)
add_test_exec(htsim_pcap_consumer_test src/htsim/pcap_consumer_test.cc ncode_htsim)
add_test_exec(htsim_flow_driver_test src/htsim/flow_driver_test.cc ncode_htsim)
add_test_exec(htsim_animator_test src/htsim/animator_test.cc ncode_htsim)
add_test_exec(htsim_bulk_gen_test src/htsim/bulk_gen_test.cc ncode_htsim)

################################
# GEO
################################
set(GEO_HEADER_FILES src/geo/geo.h)
add_library(ncode_geo STATIC src/geo/geo.cc)
target_link_libraries(ncode_geo ncode_common)

# The geonames data file is needed by the test
file(COPY data/geonames DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_test_exec(geo_test src/geo/geo_test.cc ncode_geo)

################################
# Metrics explorer
################################
add_executable(metrics_explore src/metrics/metrics_explore.cc)
target_link_libraries(metrics_explore ncode_metrics ncode_grapher)
