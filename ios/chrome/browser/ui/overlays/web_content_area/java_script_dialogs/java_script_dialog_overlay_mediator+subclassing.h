// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"

class JavaScriptDialogSource;

// Category used by JavaScriptDialogOverlayMediator subclasses to provide
// information from their OverlayRequests.
@interface JavaScriptDialogOverlayMediator (Subclassing)

// Returns the source for the OverlayRequest.
@property(nonatomic, readonly) const JavaScriptDialogSource& requestSource;

// Returns the message for the OverlayRequest.
@property(nonatomic, readonly) const std::string& requestMessage;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_SUBCLASSING_H_
