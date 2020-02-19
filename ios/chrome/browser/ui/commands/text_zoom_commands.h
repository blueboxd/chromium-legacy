// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_TEXT_ZOOM_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_TEXT_ZOOM_COMMANDS_H_

@protocol TextZoomCommands <NSObject>

// Shows the Text Zoom UI.
- (void)showTextZoom;

// Dismisses the Text Zoom UI.
- (void)hideTextZoom;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_TEXT_ZOOM_COMMANDS_H_
