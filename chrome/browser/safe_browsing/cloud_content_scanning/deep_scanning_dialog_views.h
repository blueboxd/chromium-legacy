// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageView;
class Throbber;
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
  views::View* GetContentsView() override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;

  // Updates the dialog with the result, and simply delete it from memory if it
  // nothing should be shown.
  void ShowResult(bool success);

 private:
  ~DeepScanningDialogViews() override;

  // views::DialogDelegate:
  const views::Widget* GetWidgetImpl() const override;

  // Update the UI depending on |scan_success_|.
  void UpdateDialog();

  // Resizes the already shown dialog to accommodate changes in its content.
  void Resize(int height_to_add);

  // Setup the appropriate buttons depending on |scan_success_|.
  void SetupButtons();

  // Returns a newly created side icon.
  std::unique_ptr<views::View> CreateSideIcon();

  // Returns the appropriate dialog message depending on |scan_success_|.
  base::string16 GetDialogMessage() const;

  // Returns the image's color depending on |scan_success_|.
  SkColor GetImageColor() const;

  // Returns the side image's background circle color.
  SkColor GetSideImageBackgroundColor() const;

  // Returns the appropriate dialog message depending on |scan_success_|.
  base::string16 GetCancelButtonText() const;

  // Show the dialog. Sets |shown_| to true.
  void Show();

  std::unique_ptr<DeepScanningDialogDelegate> delegate_;

  content::WebContents* web_contents_;

  // Views above the buttons. |contents_view_| owns every other view.
  std::unique_ptr<views::View> contents_view_;
  views::ImageView* image_;
  views::ImageView* side_icon_image_;
  views::Throbber* side_icon_spinner_;
  views::Label* message_;

  views::Widget* widget_;

  bool shown_ = false;

  base::TimeTicks first_shown_timestamp_;

  // Used to show the appropriate dialog depending on the scan's status.
  // base::nullopt represents a pending scan, true represents a scan with no
  // malware or DLP violation and false represents a scan with such a violation.
  base::Optional<bool> scan_success_ = base::nullopt;

  // Used to animate dialog height changes.
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  base::WeakPtrFactory<DeepScanningDialogViews> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
