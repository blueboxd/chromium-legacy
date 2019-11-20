package(
    default_visibility = ["//visibility:public"],
    features = ["-android_resources_strict_deps"],
)

licenses(["notice"])  # Apache 2

android_library(
    name = "contextmenumanager",
    srcs = glob(["*.java"]),
    manifest = "AndroidManifest.xml",
    deps = [
        "//src/main/java/com/google/android/libraries/feed/common",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/ui",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/publicapi/menumeasurer",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/ui",  # buildcleaner: keep
        "@com_google_code_findbugs_jsr305//jar",
    ],
)
