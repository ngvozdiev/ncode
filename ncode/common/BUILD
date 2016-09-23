cc_library(
    name = "hdr_util",
    srcs = [],
    hdrs = ["map_util.h", "port.h", "stl_util.h"],
    deps = [ 
    	 ":logging"
    ]
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    deps = [
      ":common",
      ":logging",
      ":strutil"
    ],
)

cc_library(
    name = "strutil", 
    srcs = ["strutil.cc"],
    hdrs = ["strutil.h"],
    deps = [
      ":hdr_util",
      ":stringpiece",
      ":logging",
    ],
)

cc_test(
  name = "strutil_test",
  srcs = [
    "strutil_test.cc",
  ],
  deps = [
    "//third_party/gtest:gtest_main",
    ":hdr_util",
    ":stringpiece",
    ":strutil",
    ":logging",
  ],
)

cc_library(
    name = "stringpiece", 
    srcs = ["stringpiece.cc"],
    hdrs = ["stringpiece.h"],
    deps = [
      ":logging",
    ],
)

cc_test(
  name = "stringpiece_test",
  srcs = [
    "stringpiece_test.cc",
  ],
  deps = [
    ":stringpiece",
    "//third_party/gtest:gtest_main",
  ],
)

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
)

cc_test(
  name = "logging_test",
  srcs = [
    "logging_test.cc",
  ],
  deps = [
    ":common",
    ":file",
    ":logging",
    ":substitute",
    "//third_party/gtest:gtest_main",
  ],
)

cc_library(
  name = "common",
  srcs = ["common.cc"],
  hdrs = ["common.h"],
  deps = [
    ":hdr_util",
    ":logging",
    "//third_party/protobuf:protobuf"
  ],
)

cc_test(
  name = "common_test",
  srcs = [
    "common_test.cc",
  ],
  deps = [
    ":common",
    ":hdr_util",
    ":logging",
    "//third_party/protobuf:protobuf",
    "//third_party/gtest:gtest_main",
  ],
)

cc_library(
  name = "substitute",
  srcs = ["substitute.cc"],
  hdrs = ["substitute.h"],
  deps = [
    ":hdr_util",
    ":strutil",
  ],
)

cc_library(
  name = "event_queue",
  srcs = ["event_queue.cc"],
  hdrs = ["event_queue.h"],
  deps = [
    ":common",
    ":logging",
  ],
)

cc_test(
  name = "event_queue_test",
  srcs = [
    "event_queue_test.cc",
  ],
  deps = [
    ":common",
    ":event_queue",
    ":logging",
    "//third_party/gtest:gtest_main",
  ],
)
