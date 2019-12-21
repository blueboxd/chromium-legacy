// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/chrome_javascript_app_modal_dialog_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_native_app_modal_dialog_factory.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

void ChromeJavascriptAppModalDialogAndroid::ShowAppModalDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TabAndroid* tab = TabAndroid::FromWebContents(dialog()->web_contents());
  if (!tab) {
    CancelAppModalDialog();
    return;
  }

  DoShowAppModalDialog(tab->IsUserInteractable());
}

ChromeJavascriptAppModalDialogAndroid::
    ~ChromeJavascriptAppModalDialogAndroid() = default;

namespace {

class ChromeJavaScriptNativeDialogAndroidFactory
    : public app_modal::JavaScriptNativeDialogFactory {
 public:
  ChromeJavaScriptNativeDialogAndroidFactory() = default;
  ~ChromeJavaScriptNativeDialogAndroidFactory() override = default;

 private:
  app_modal::NativeAppModalDialog* CreateNativeJavaScriptDialog(
      app_modal::JavaScriptAppModalDialog* dialog) override {
    dialog->web_contents()->GetDelegate()->ActivateContents(
        dialog->web_contents());
    return new ChromeJavascriptAppModalDialogAndroid(
        base::android::AttachCurrentThread(), dialog,
        dialog->web_contents()->GetTopLevelNativeWindow());
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptNativeDialogAndroidFactory);
};

}  // namespace

void InstallChromeJavaScriptNativeAppModalDialogFactory() {
  app_modal::JavaScriptDialogManager::GetInstance()->SetNativeDialogFactory(
      base::WrapUnique(new ChromeJavaScriptNativeDialogAndroidFactory));
}
