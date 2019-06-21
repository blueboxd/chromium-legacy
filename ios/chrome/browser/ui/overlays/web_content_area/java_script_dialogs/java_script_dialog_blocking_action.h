// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_BLOCKING_ACTION_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_BLOCKING_ACTION_H_

#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"

@class JavaScriptDialogOverlayMediator;

// Returns the dialog blocking option for a JavaScript dialog being configured
// by |mediator|, which is expected to be non-nil.  Returns nil if no blocking
// option should be added to the dialog.
AlertAction* GetBlockingAlertAction(JavaScriptDialogOverlayMediator* mediator);

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_BLOCKING_ACTION_H_
