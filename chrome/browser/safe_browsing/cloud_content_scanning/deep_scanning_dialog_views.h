// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_

#include <memory>

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class MessageBoxView;
class Widget;
}  // namespace views

namespace safe_browsing {

// Dialog shown for Deep Scanning to offer the possibility of cancelling the
// upload to the user.
class DeepScanningDialogViews : public views::DialogDelegate {
 public:
  DeepScanningDialogViews(std::unique_ptr<DeepScanningDialogDelegate> delegate,
                          content::WebContents* web_contents);

  // views::DialogDelegate:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;

 private:
  ~DeepScanningDialogViews() override;

  std::unique_ptr<DeepScanningDialogDelegate> delegate_;

  views::MessageBoxView* message_box_view_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
