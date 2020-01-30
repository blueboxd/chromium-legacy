// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_ANDROID_H_

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper_delegate.h"

namespace content {
class WebContents;
}

class JavaScriptDialogTabHelperDelegateAndroid
    : public JavaScriptDialogTabHelperDelegate {
 public:
  explicit JavaScriptDialogTabHelperDelegateAndroid(
      content::WebContents* web_contents);
  ~JavaScriptDialogTabHelperDelegateAndroid() override;

  JavaScriptDialogTabHelperDelegateAndroid(
      const JavaScriptDialogTabHelperDelegateAndroid& other) = delete;
  JavaScriptDialogTabHelperDelegateAndroid& operator=(
      const JavaScriptDialogTabHelperDelegateAndroid& other) = delete;

  // JavaScriptDialogTabHelperDelegate
  base::WeakPtr<JavaScriptDialog> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;

 private:
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_ANDROID_H_
