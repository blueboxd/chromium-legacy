android_sdk_repository(
    name = "androidsdk"
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Google Maven Repository
GMAVEN_RULES_JVM_EXTERNAL_TAG = "1.0"
GMAVEN_RULES_JVM_EXTERNAL_SHA = "48e0f1aab74fabba98feb8825459ef08dcc75618d381dff63ec9d4dd9860deaa"

# Current version of Android support library which Android code depends on
ANDROID_SUPPORT_LIBRARY_VERSION = "28.0.0"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % GMAVEN_RULES_JVM_EXTERNAL_TAG,
    sha256 = GMAVEN_RULES_JVM_EXTERNAL_SHA,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % GMAVEN_RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:defs.bzl", "maven_install")
load("//third_party:dependencies.bzl", "get_robolectric_dependency")

maven_install(
    artifacts = [
        "androidx.test:core:1.1.0",
        "androidx.test:monitor:1.1.0",
        "com.android.support:appcompat-v7:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:collections:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:interpolator:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:recyclerview-v7:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:support-annotations:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:support-compat:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:support-core-ui:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.android.support:swiperefreshlayout:" + ANDROID_SUPPORT_LIBRARY_VERSION,
        "com.google.dagger:dagger:2.17",
        "com.google.dagger:dagger-compiler:2.17",
        "com.google.dagger:dagger-producers:2.17",
        "com.google.guava:guava:27.0.1-android",
        "com.google.guava:guava-testlib:27.0.1-android",
        "com.google.truth:truth:0.45",
        "junit:junit:4.12",
        "org.mockito:mockito-core:2.24.0",
        get_robolectric_dependency("annotations"),
        get_robolectric_dependency("robolectric"),
        get_robolectric_dependency("shadowapi"),
        get_robolectric_dependency("shadows-framework"),
    ],
    repositories = [
        "http://central.maven.org/maven2/",
        "https://maven.google.com",
    ],
)

# Loading android_library rules for Bazel scripts.
http_archive(
    name = "build_bazel_rules_android",
    urls = ["https://github.com/bazelbuild/rules_android/archive/v0.1.1.zip"],
    sha256 = "cd06d15dd8bb59926e4d65f9003bfc20f9da4b2519985c27e190cddc8b7a7806",
    strip_prefix = "rules_android-0.1.1",
)

http_archive(
    name = "com_google_protobuf",
    sha256 = "73fdad358857e120fd0fa19e071a96e15c0f23bb25f85d3f7009abfd4f264a2a",
    strip_prefix = "protobuf-3.6.1.3",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.6.1.3.tar.gz"],
)

http_archive(
    name = "com_google_protobuf_javalite",
    sha256 = "79d102c61e2a479a0b7e5fc167bcfaa4832a0c6aad4a75fa7da0480564931bcc",
    strip_prefix = "protobuf-384989534b2246d413dbcd750744faab2607b516",
    urls = ["https://github.com/google/protobuf/archive/384989534b2246d413dbcd750744faab2607b516.zip"],
)

http_archive(
    name = "robolectric",
    urls = ["https://github.com/robolectric/robolectric-bazel/archive/4.1.tar.gz"],
    sha256 = "2ee850ca521288db72b0dedb9ecbda55b64d11c470435a882f8daf615091253d",
    strip_prefix = "robolectric-bazel-4.1",
)

load("@robolectric//bazel:robolectric.bzl", "robolectric_repositories")
robolectric_repositories()

maven_jar(
    name = "com_google_android_android",
    artifact = "com.google.android:android:4.1.1.4",
)

maven_jar(
    name = "com_google_code_findbugs_jsr305",
    artifact = "com.google.code.findbugs:jsr305:3.0.2",
)

maven_jar(
    name = "com_google_dagger_dagger",
    artifact = "com.google.dagger:dagger:2.17",
)

maven_jar(
    name = "com_google_inject_guice",
    artifact = "com.google.inject:guice:4.2.2",
)

maven_jar(
    name = "com_google_testing_compile_compile_testing",
    artifact = "com.google.testing.compile:compile-testing:0.15",
)

maven_jar(
    name = "com_google_truth_liteproto_extension",
    artifact = "com.google.truth.extensions:truth-liteproto-extension:0.45"
)

maven_jar(
    name = "com_squareup_javapoet",
    artifact = "com.squareup:javapoet:1.8.0",
)

maven_jar(
    name = "javax_annotation_jsr250_api",
    artifact = "javax.annotation:jsr250-api:1.0",
)

maven_jar(
    name = "javax_inject_javax_inject",
    artifact = "javax.inject:javax.inject:1",
)
