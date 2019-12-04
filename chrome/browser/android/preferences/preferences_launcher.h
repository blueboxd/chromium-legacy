// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREFERENCES_LAUNCHER_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREFERENCES_LAUNCHER_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

namespace chrome {
namespace android {

class PreferencesLauncher {
 public:
  // Opens the autofill settings page for profiles.
  static void ShowAutofillProfileSettings(content::WebContents* web_contents);

  // Opens the autofill settings page for credit cards.
  static void ShowAutofillCreditCardSettings(
      content::WebContents* web_contents);

 private:
  PreferencesLauncher() {}
  ~PreferencesLauncher() {}

  DISALLOW_COPY_AND_ASSIGN(PreferencesLauncher);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREFERENCES_LAUNCHER_H_
