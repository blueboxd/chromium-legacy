// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantColor_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDimension_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantValue_jni.h"

namespace autofill_assistant {

namespace ui_controller_android_utils {

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const std::string& color_string) {
  if (!Java_AssistantColor_isValidColorString(
          env, base::android::ConvertUTF8ToJavaString(env, color_string))) {
    if (!color_string.empty()) {
      DVLOG(1) << "Encountered invalid color string: " << color_string;
    }
    return nullptr;
  }

  return Java_AssistantColor_createFromString(
      env, base::android::ConvertUTF8ToJavaString(env, color_string));
}

base::android::ScopedJavaLocalRef<jobject> GetJavaColor(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ColorProto& proto) {
  switch (proto.color_case()) {
    case ColorProto::kResourceIdentifier:
      if (!Java_AssistantColor_isValidResourceIdentifier(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        DVLOG(1) << "Encountered invalid color resource identifier: "
                 << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantColor_createFromResource(
          env, jcontext,
          base::android::ConvertUTF8ToJavaString(env,
                                                 proto.resource_identifier()));
    case ColorProto::kParseableColor:
      return GetJavaColor(env, proto.parseable_color());
    case ColorProto::COLOR_NOT_SET:
      return nullptr;
  }
}

base::Optional<int> GetPixelSize(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto) {
  switch (proto.size_case()) {
    case ClientDimensionProto::kDp:
      return Java_AssistantDimension_getPixelSizeDp(env, jcontext, proto.dp());
      break;
    case ClientDimensionProto::kWidthFactor:
      return Java_AssistantDimension_getPixelSizeWidthFactor(
          env, jcontext, proto.width_factor());
      break;
    case ClientDimensionProto::kHeightFactor:
      return Java_AssistantDimension_getPixelSizeHeightFactor(
          env, jcontext, proto.height_factor());
      break;
    case ClientDimensionProto::SIZE_NOT_SET:
      return base::nullopt;
  }
}

int GetPixelSizeOrDefault(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const ClientDimensionProto& proto,
    int default_value) {
  auto size = GetPixelSize(env, jcontext, proto);
  if (size) {
    return *size;
  }
  return default_value;
}

base::android::ScopedJavaLocalRef<jobject> ToJavaValue(
    JNIEnv* env,
    const ValueProto& proto) {
  switch (proto.kind_case()) {
    case ValueProto::kStrings: {
      std::vector<std::string> strings;
      strings.reserve(proto.strings().values_size());
      for (const auto& string : proto.strings().values()) {
        strings.push_back(string);
      }
      return Java_AssistantValue_createForStrings(
          env, base::android::ToJavaArrayOfStrings(env, strings));
    }
    case ValueProto::kBooleans: {
      auto booleans = std::make_unique<bool[]>(proto.booleans().values_size());
      for (int i = 0; i < proto.booleans().values_size(); ++i) {
        booleans[i] = proto.booleans().values()[i];
      }
      return Java_AssistantValue_createForBooleans(
          env, base::android::ToJavaBooleanArray(
                   env, booleans.get(), proto.booleans().values_size()));
    }
    case ValueProto::kInts: {
      auto ints = std::make_unique<int[]>(proto.ints().values_size());
      for (int i = 0; i < proto.ints().values_size(); ++i) {
        ints[i] = proto.ints().values()[i];
      }
      return Java_AssistantValue_createForIntegers(
          env, base::android::ToJavaIntArray(env, ints.get(),
                                             proto.ints().values_size()));
    }
    case ValueProto::KIND_NOT_SET:
      return Java_AssistantValue_create(env);
  }
}

ValueProto ToNativeValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jvalue) {
  ValueProto proto;
  auto jints = Java_AssistantValue_getIntegers(env, jvalue);
  if (jints) {
    std::vector<int> ints;
    base::android::JavaIntArrayToIntVector(env, jints, &ints);
    proto.mutable_ints();
    for (int i : ints) {
      proto.mutable_ints()->add_values(i);
    }
    return proto;
  }

  auto jbooleans = Java_AssistantValue_getBooleans(env, jvalue);
  if (jbooleans) {
    std::vector<bool> booleans;
    base::android::JavaBooleanArrayToBoolVector(env, jbooleans, &booleans);
    proto.mutable_booleans();
    for (auto b : booleans) {
      proto.mutable_booleans()->add_values(b);
    }
    return proto;
  }

  auto jstrings = Java_AssistantValue_getStrings(env, jvalue);
  if (jstrings) {
    std::vector<std::string> strings;
    base::android::AppendJavaStringArrayToStringVector(env, jstrings, &strings);
    proto.mutable_strings();
    for (const auto& string : strings) {
      proto.mutable_strings()->add_values(string);
    }
    return proto;
  }

  return proto;
}

}  // namespace ui_controller_android_utils

}  // namespace autofill_assistant
