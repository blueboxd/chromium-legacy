package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "feedrequestmanager",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/requestmanager",
        "//src/main/java/com/google/android/libraries/feed/api/common",
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//src/main/java/com/google/android/libraries/feed/api/host/scheduler",
        "//src/main/java/com/google/android/libraries/feed/api/host/storage",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/api/internal/actionmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/api/internal/protocoladapter",
        "//src/main/java/com/google/android/libraries/feed/api/internal/requestmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/sessionmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/common/locale",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/protoextensions",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/java/com/google/android/libraries/feed/feedrequestmanager/internal",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
    ],
)
