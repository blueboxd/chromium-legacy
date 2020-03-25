// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_EVENTS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_EVENTS_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/interactions.pb.h"

namespace autofill_assistant {
namespace android_events {

// Sets a click listener for |jview|.
void SetOnClickListener(JNIEnv* env,
                        base::android::ScopedJavaGlobalRef<jobject> jview,
                        base::android::ScopedJavaGlobalRef<jobject> jdelegate,
                        const OnViewClickedEventProto& proto);

}  // namespace android_events
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_EVENTS_ANDROID_H_
