package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "common",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
    ],
)
