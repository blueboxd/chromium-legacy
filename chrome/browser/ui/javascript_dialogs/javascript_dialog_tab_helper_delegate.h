// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/javascript_dialog_manager.h"

class JavaScriptDialog;

// This interface provides platform-specific controller functionality to
// JavaScriptDialogTabHelper.
class JavaScriptDialogTabHelperDelegate {
 public:
  virtual ~JavaScriptDialogTabHelperDelegate() = default;

  // Factory function for creating a tab modal view.
  virtual base::WeakPtr<JavaScriptDialog> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) = 0;

  // Called when a dialog is about to be shown.
  virtual void WillRunDialog() = 0;

  // Called when a dialog has been hidden.
  virtual void DidCloseDialog() = 0;

  // Called when a tab should indicate to the user that it needs attention, such
  // as when an alert fires from a background tab.
  virtual void SetTabNeedsAttention(bool attention) = 0;

  // Should return true if the web contents is foremost (i.e. the active tab in
  // the active browser window).
  virtual bool IsWebContentsForemost() = 0;

  // Should return true if this web contents is an app window, such as a PWA.
  virtual bool IsApp() = 0;
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_TAB_HELPER_DELEGATE_H_
