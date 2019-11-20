package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

proto_library(
    name = "scroll_state",
    srcs = ["scroll_state.proto"],
    deps = [],
)

java_lite_proto_library(
    name = "scroll_state_java_proto_lite",
    deps = [":scroll_state"],
)

proto_library(
    name = "ui_refresh_reason",
    srcs = ["ui_refresh_reason.proto"],
    deps = [
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_proto",
    ],
)

java_lite_proto_library(
    name = "ui_refresh_reason_java_proto_lite",
    deps = [":ui_refresh_reason"],
)
