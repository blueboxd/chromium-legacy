package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

exports_files(["LICENSE"])

proto_library(
    name = "feed_proto",
    srcs = glob(["*.proto"]),
    deps = [
        "//src/main/proto/search/now/ui/piet:piet_proto",
    ],
)

cc_proto_library(
    name = "feed_cc_proto",
    deps = [":feed_proto"],
)

java_proto_library(
    name = "feed_java_proto",
    deps = [":feed_proto"],
)

java_lite_proto_library(
    name = "feed_java_proto_lite",
    deps = [":feed_proto"],
)
