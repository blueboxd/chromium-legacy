// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_LITE_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_LITE_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {

// Creates a new |LiteService| instance that will notify |java_lite_service|
// upon script completion or failure.
static jlong JNI_AutofillAssistantLiteService_CreateLiteService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_lite_service,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jtrigger_script_path);

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_LITE_SERVICE_BRIDGE_H_
