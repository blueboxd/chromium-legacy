package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

proto_library(
    name = "stream_saved_instance_state",
    srcs = ["stream_saved_instance_state.proto"],
    deps = [
        "//src/main/proto/com/google/android/libraries/feed/sharedstream/proto:scroll_state",
    ],
)

java_lite_proto_library(
    name = "stream_saved_instance_state_java_proto_lite",
    deps = [":stream_saved_instance_state"],
)
