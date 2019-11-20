package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "network",
    testonly = 1,
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//src/main/java/com/google/android/libraries/feed/common/testing",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)
