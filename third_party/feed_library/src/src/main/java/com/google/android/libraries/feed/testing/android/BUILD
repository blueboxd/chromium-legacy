package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "android",
    testonly = 1,
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/common",
        "@maven//:com_android_support_recyclerview_v7",
    ],
)
