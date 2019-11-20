package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "logging",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_support_annotations",
        "@maven//:com_android_support_support_compat",  # buildcleaner: keep
        "@maven//:com_android_support_support_core_ui",
    ],
)
