// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_

namespace sharesheet {

// The SharesheetController allows ShareActions to request changes to the state
// of the sharesheet.
class SharesheetController {
 public:
  virtual ~SharesheetController() = default;

  // Called by ShareAction to notify SharesheetBubbleView that ShareAction
  // has completed.
  virtual void ShareActionCompleted() = 0;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
