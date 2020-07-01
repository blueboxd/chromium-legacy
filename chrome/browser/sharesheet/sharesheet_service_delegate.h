// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_

#include <memory>

class SharesheetBubbleView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}

namespace sharesheet {

class SharesheetService;

class SharesheetServiceDelegate {
 public:
  explicit SharesheetServiceDelegate(content::WebContents* web_contents,
                                     views::View* bubble_anchor_view);
  ~SharesheetServiceDelegate();
  SharesheetServiceDelegate(const SharesheetServiceDelegate&) = delete;
  SharesheetServiceDelegate& operator=(const SharesheetServiceDelegate&) =
      delete;

  void ShowBubble();

 private:
  std::unique_ptr<SharesheetBubbleView> sharesheet_bubble_view_;
  std::unique_ptr<SharesheetService> sharesheet_service_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
