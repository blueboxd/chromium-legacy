// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "components/app_modal/android/javascript_app_modal_dialog_android.h"

class ChromeJavascriptAppModalDialogAndroid
    : public app_modal::JavascriptAppModalDialogAndroid {
 public:
  using JavascriptAppModalDialogAndroid::JavascriptAppModalDialogAndroid;

  // NativeAppModalDialog:
  void ShowAppModalDialog() override;

 protected:
  ~ChromeJavascriptAppModalDialogAndroid() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeJavascriptAppModalDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CHROME_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
