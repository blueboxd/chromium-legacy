// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_

#include <memory>

#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"

class SharesheetBubbleView;

namespace views {
class View;
}

namespace sharesheet {

class SharesheetService;

// The SharesheetServiceDelegate is the middle point between the UI and the
// business logic in the sharesheet.
class SharesheetServiceDelegate : public SharesheetController {
 public:
  explicit SharesheetServiceDelegate(uint32_t id,
                                     views::View* bubble_anchor_view,
                                     SharesheetService* sharesheet_service);
  ~SharesheetServiceDelegate() override;
  SharesheetServiceDelegate(const SharesheetServiceDelegate&) = delete;
  SharesheetServiceDelegate& operator=(const SharesheetServiceDelegate&) =
      delete;

  void ShowBubble(std::vector<TargetInfo> targets);
  void OnBubbleClosed();

  // SharesheetController overrides
  uint32_t GetId() override;
  void ShareActionCompleted() override;

 private:
  const uint32_t id_;
  std::unique_ptr<SharesheetBubbleView> sharesheet_bubble_view_;
  SharesheetService* sharesheet_service_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
