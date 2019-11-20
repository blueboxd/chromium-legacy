package(
    default_visibility = ["//visibility:public"],
    features = ["-android_resources_strict_deps"],
)

licenses(["notice"])  # Apache 2

android_library(
    name = "piet",
    srcs = glob(["*.java"]),
    manifest = "AndroidManifest.xml",
    resource_files = glob(["res/**/*"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/imageloader",
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/time",
        "//src/main/java/com/google/android/libraries/feed/common/ui",
        "//src/main/java/com/google/android/libraries/feed/piet/host",
        "//src/main/java/com/google/android/libraries/feed/piet/ui",
        "//src/main/proto/search/now/ui/piet:piet_errors_java_proto_lite",
        "//src/main/proto/search/now/ui/piet:piet_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_support_annotations",  # buildcleaner: keep
        "@maven//:com_android_support_support_compat",
    ],
)
