package(
    default_visibility = ["//visibility:public"],
    features = ["-android_resources_strict_deps"],
)

licenses(["notice"])  # Apache 2

android_library(
    name = "ui",
    srcs = glob(["*.java"]),
    manifest = "AndroidManifest.xml",
    resource_files = glob(["res/**"]),
    deps = [
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_appcompat_v7",
        "@maven//:com_android_support_swiperefreshlayout",
    ],
)
