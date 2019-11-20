package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "protoextensions",
    srcs = glob(["*.java"]),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/proto",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "//src/main/proto/search/now/ui/action:feed_action_java_proto_lite",
        "//src/main/proto/search/now/ui/action:piet_extensions_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_offline_extension_java_proto_lite",
        "//src/main/proto/search/now/ui/stream:stream_swipe_extension_java_proto_lite",
        "//src/main/proto/search/now/wire/feed:feed_java_proto_lite",
        "@com_google_protobuf_javalite//:protobuf_java_lite",
    ],
)
