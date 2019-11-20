package(
    default_visibility = ["//visibility:public"],
    features = ["-android_resources_strict_deps"],
)

licenses(["notice"])  # Apache 2

android_library(
    name = "viewholders",
    srcs = glob(["*.java"]),
    manifest = "AndroidManifest.xml",
    resource_files = glob(["res/**"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/stream",
        "//src/main/java/com/google/android/libraries/feed/api/host/action",
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/api/internal/actionparser",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/ui",
        "//src/main/java/com/google/android/libraries/feed/piet",
        "//src/main/java/com/google/android/libraries/feed/piet/host",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/logging",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/piet",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/publicapi/scroll",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/scroll",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/ui",  # buildcleaner: keep
        "//src/main/proto/search/now/ui/action:feed_action_payload_java_proto_lite",
        "//src/main/proto/search/now/ui/piet:piet_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_recyclerview_v7",
        "@maven//:com_android_support_support_annotations",  # buildcleaner: keep
    ],
)
