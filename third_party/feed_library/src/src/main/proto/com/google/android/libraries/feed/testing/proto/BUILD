package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

proto_library(
    name = "ui_context_for_test",
    testonly = 1,
    srcs = ["ui_context_for_test.proto"],
    deps = [
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_proto",
    ],
)

java_lite_proto_library(
    name = "ui_refresh_reason_java_proto_lite",
    testonly = 1,
    deps = [":ui_context_for_test"],
)
