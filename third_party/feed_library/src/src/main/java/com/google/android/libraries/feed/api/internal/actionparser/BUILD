package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "actionparser",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/knowncontent",
        "//src/main/java/com/google/android/libraries/feed/api/host/action",
        "//src/main/java/com/google/android/libraries/feed/common/functional",
        "//src/main/java/com/google/android/libraries/feed/piet/host",
        "//src/main/proto/search/now/ui/action:feed_action_payload_java_proto_lite",
        "//src/main/proto/search/now/ui/piet:piet_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
        "@maven//:com_android_support_support_annotations",
    ],
)
