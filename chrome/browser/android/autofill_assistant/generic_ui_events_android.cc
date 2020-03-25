// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/generic_ui_events_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewEvents_jni.h"

namespace autofill_assistant {
namespace android_events {

void SetOnClickListener(JNIEnv* env,
                        base::android::ScopedJavaGlobalRef<jobject> jview,
                        base::android::ScopedJavaGlobalRef<jobject> jdelegate,
                        const OnViewClickedEventProto& proto) {
  Java_AssistantViewEvents_setOnClickListener(
      env, jview,
      base::android::ConvertUTF8ToJavaString(env, proto.view_identifier()),
      jdelegate);
}

}  // namespace android_events
}  // namespace autofill_assistant
