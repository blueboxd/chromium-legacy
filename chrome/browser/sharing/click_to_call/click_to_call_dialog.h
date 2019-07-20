// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_H_

// The cross-platform UI interface which displays the click to call dialog.
// This object is responsible for its own lifetime.
class ClickToCallDialog {
 public:
  // Called to close the dialog and prevent future callbacks into the
  // controller.
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_DIALOG_H_
