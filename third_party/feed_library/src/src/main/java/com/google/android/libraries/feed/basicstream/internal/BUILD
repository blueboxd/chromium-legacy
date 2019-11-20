package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "internal",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/stream",
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/drivers",
        "//src/main/java/com/google/android/libraries/feed/basicstream/internal/viewholders",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/piet",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/deepestcontenttracker",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/piet",
        "//src/main/java/com/google/android/libraries/feed/sharedstream/publicapi/scroll",
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_interpolator",
        "@maven//:com_android_support_recyclerview_v7",
        "@maven//:com_android_support_support_annotations",  # buildcleaner: keep
    ],
)
