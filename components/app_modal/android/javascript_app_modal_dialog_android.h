// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
#define COMPONENTS_APP_MODAL_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace app_modal {

class JavaScriptAppModalDialog;

class JavascriptAppModalDialogAndroid : public NativeAppModalDialog {
 public:
  JavascriptAppModalDialogAndroid(JNIEnv* env,
                                  JavaScriptAppModalDialog* dialog,
                                  gfx::NativeWindow parent);

  // NativeAppModalDialog:
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // Called when java confirms or cancels the dialog.
  void DidAcceptAppModalDialog(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& prompt_text,
      bool suppress_js_dialogs);
  void DidCancelAppModalDialog(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>&,
                               bool suppress_js_dialogs);

  const base::android::ScopedJavaGlobalRef<jobject>& GetDialogObject() const;

 protected:
  JavaScriptAppModalDialog* dialog() { return dialog_.get(); }

  ~JavascriptAppModalDialogAndroid() override;

 private:
  std::unique_ptr<JavaScriptAppModalDialog> dialog_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef parent_jobject_weak_ref_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptAppModalDialogAndroid);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
