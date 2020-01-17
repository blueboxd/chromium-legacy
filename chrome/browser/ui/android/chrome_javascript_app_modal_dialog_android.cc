// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_native_app_modal_dialog_factory.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "components/app_modal/android/javascript_app_modal_dialog_android.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"

void InstallChromeJavaScriptNativeAppModalDialogFactory() {
  app_modal::JavaScriptDialogManager::GetInstance()->SetNativeDialogFactory(
      base::BindRepeating([](app_modal::JavaScriptAppModalDialog* dialog) {
        app_modal::NativeAppModalDialog* d =
            new app_modal::JavascriptAppModalDialogAndroid(
                base::android::AttachCurrentThread(), dialog,
                dialog->web_contents()->GetTopLevelNativeWindow());
        return d;
      }));
}
