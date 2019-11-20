package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

android_library(
    name = "scope",
    srcs = glob(
        ["*.java"],
        exclude = ["*Scope.java"],
    ),
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/api/internal/lifecycle",
        "//src/main/java/com/google/android/libraries/feed/api/internal/sessionmanager",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/feedobservable",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/feedapplifecyclelistener",
        "//src/main/proto/com/google/android/libraries/feed/api/internal/proto:client_feed_java_proto_lite",
        "@com_google_code_findbugs_jsr305//jar",
    ],
)

android_library(
    name = "feedprocessscope",
    srcs = ["FeedProcessScope.java"],
    deps = [
        ":scope",
        "//src/main/java/com/google/android/libraries/feed/api/client/knowncontent",
        "//src/main/java/com/google/android/libraries/feed/api/client/lifecycle",
        "//src/main/java/com/google/android/libraries/feed/api/client/requestmanager",
        "//src/main/java/com/google/android/libraries/feed/api/client/scope:processscope",
        "//src/main/java/com/google/android/libraries/feed/api/client/scope:streamscopebuilder",
        "//src/main/java/com/google/android/libraries/feed/api/host/action",
        "//src/main/java/com/google/android/libraries/feed/api/host/config",
        "//src/main/java/com/google/android/libraries/feed/api/host/imageloader",
        "//src/main/java/com/google/android/libraries/feed/api/host/logging",
        "//src/main/java/com/google/android/libraries/feed/api/host/network",
        "//src/main/java/com/google/android/libraries/feed/api/host/offlineindicator",
        "//src/main/java/com/google/android/libraries/feed/api/host/stream",
        "//src/main/java/com/google/android/libraries/feed/api/internal/actionmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/common",
        "//src/main/java/com/google/android/libraries/feed/api/internal/knowncontent",
        "//src/main/java/com/google/android/libraries/feed/api/internal/protocoladapter",
        "//src/main/java/com/google/android/libraries/feed/api/internal/sessionmanager",
        "//src/main/java/com/google/android/libraries/feed/api/internal/store",
        "//src/main/java/com/google/android/libraries/feed/basicstream",
        "//src/main/java/com/google/android/libraries/feed/common/concurrent",
        "//src/main/java/com/google/android/libraries/feed/common/logging",
        "//src/main/java/com/google/android/libraries/feed/common/protoextensions",
        "//src/main/java/com/google/android/libraries/feed/common/time",
    ],
)

android_library(
    name = "feedstreamscope",
    srcs = ["FeedStreamScope.java"],
    deps = [
        "//src/main/java/com/google/android/libraries/feed/api/client/scope:streamscope",
        "//src/main/java/com/google/android/libraries/feed/api/client/stream",
        "//src/main/java/com/google/android/libraries/feed/api/internal/modelprovider",
        "//src/main/java/com/google/android/libraries/feed/basicstream",
    ],
)
