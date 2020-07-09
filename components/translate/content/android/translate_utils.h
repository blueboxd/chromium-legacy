// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_
#define COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace translate {

class TranslateInfoBarDelegate;

class TranslateUtils {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.translate
  // GENERATED_JAVA_PREFIX_TO_STRIP:OPTION_
  enum TranslateOption {
    OPTION_SOURCE_CODE,
    OPTION_TARGET_CODE,
    OPTION_ALWAYS_TRANSLATE,
    OPTION_NEVER_TRANSLATE,
    OPTION_NEVER_TRANSLATE_SITE
  };

  static base::android::ScopedJavaLocalRef<jobjectArray> GetJavaLanguages(
      JNIEnv* env,
      TranslateInfoBarDelegate* delegate);
  static base::android::ScopedJavaLocalRef<jobjectArray> GetJavaLanguageCodes(
      JNIEnv* env,
      TranslateInfoBarDelegate* delegate);
  static base::android::ScopedJavaLocalRef<jintArray> GetJavaLanguageHashCodes(
      JNIEnv* env,
      TranslateInfoBarDelegate* delegate);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_UTILS_H_
