// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/preferences_launcher.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PreferencesLauncher_jni.h"
#include "content/public/browser/web_contents.h"

namespace chrome {
namespace android {

void PreferencesLauncher::ShowAutofillProfileSettings(
    content::WebContents* web_contents) {
  Java_PreferencesLauncher_showAutofillProfileSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents());
}

void PreferencesLauncher::ShowAutofillCreditCardSettings(
    content::WebContents* web_contents) {
  Java_PreferencesLauncher_showAutofillCreditCardSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents());
}

}  // namespace android
}  // namespace chrome
