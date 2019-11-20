package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2

java_library(
    name = "dagger",
    exported_plugins = [":dagger_plugin"],
    exports = [
        "@maven//:com_google_dagger_dagger",
    ],
)

java_plugin(
    name = "dagger_plugin",
    generates_api = 1,
    processor_class = "dagger.internal.codegen.ComponentProcessor",
    deps = [
        "@maven//:com_google_dagger_dagger",
        "@maven//:com_google_dagger_dagger_compiler",
        "@maven//:com_google_dagger_dagger_producers",
    ],
)

java_library(
    name = "robolectric",
    exports = [
        "@maven//:org_robolectric_annotations",
        "@maven//:org_robolectric_robolectric",
        "@maven//:org_robolectric_shadowapi",
        "@maven//:org_robolectric_shadows_framework",
    ],
)
