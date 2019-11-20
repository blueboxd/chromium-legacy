package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "feedactionreader",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/internal/actionmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/api/internal/protocoladapter",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
    ],
)
