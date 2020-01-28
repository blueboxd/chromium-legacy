// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class ImageView;
class Throbber;
class Widget;
}  // namespace views

namespace safe_browsing {
class DeepScanningTopImageView;

// Dialog shown for Deep Scanning to offer the possibility of cancelling the
// upload to the user.
class DeepScanningDialogViews : public views::DialogDelegate {
 public:
  // Enum used to represent what the dialog is currently showing.
  enum class DeepScanningDialogStatus {
    // The dialog is shown with an explanation that the scan is being performed
    // and that the result is pending.
    PENDING,

    // The dialog is shown with a short message indicating that the scan was a
    // success and that the user may proceed with their upload, drag-and-drop or
    // paste.
    SUCCESS,

    // The dialog is shown with a message indicating that the scan was a failure
    // and that the user may not proceed with their upload, drag-and-drop or
    // paste.
    FAILURE,
  };
  DeepScanningDialogViews(std::unique_ptr<DeepScanningDialogDelegate> delegate,
                          content::WebContents* web_contents,
                          base::Optional<DeepScanAccessPoint> access_point,
                          bool is_file_scan);

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;
  views::View* GetContentsView() override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;

  // Updates the dialog with the result, and simply delete it from memory if it
  // nothing should be shown.
  void ShowResult(bool success);

  // Returns the appropriate top image depending on |dialog_status_|.
  const gfx::ImageSkia* GetTopImage() const;

  // Accessors to simplify |dialog_status_| checking.
  inline bool is_success() const {
    return dialog_status_ == DeepScanningDialogStatus::SUCCESS;
  }

  inline bool is_failure() const {
    return dialog_status_ == DeepScanningDialogStatus::FAILURE;
  }

  inline bool is_result() const { return is_success() || is_failure(); }

  inline bool is_pending() const {
    return dialog_status_ == DeepScanningDialogStatus::PENDING;
  }

 private:
  ~DeepScanningDialogViews() override;

  // views::DialogDelegate:
  const views::Widget* GetWidgetImpl() const override;

  // Update the UI depending on |dialog_status_|.
  void UpdateDialog();

  // Resizes the already shown dialog to accommodate changes in its content.
  void Resize(int height_to_add);

  // Setup the appropriate buttons depending on |dialog_status_|.
  void SetupButtons();

  // Returns a newly created side icon.
  std::unique_ptr<views::View> CreateSideIcon();

  // Returns the appropriate dialog message depending on |dialog_status_|.
  base::string16 GetDialogMessage() const;

  // Returns the side image's background circle color.
  SkColor GetSideImageBackgroundColor() const;

  // Returns the appropriate dialog message depending on |dialog_status_|.
  base::string16 GetCancelButtonText() const;

  // Returns the appropriate paste top image ID depending on |dialog_status_|.
  int GetPasteImageId(bool use_dark) const;

  // Returns the appropriate upload top image ID depending on |dialog_status_|.
  int GetUploadImageId(bool use_dark) const;

  // Show the dialog. Sets |shown_| to true.
  void Show();

  std::unique_ptr<DeepScanningDialogDelegate> delegate_;

  content::WebContents* web_contents_;

  // Views above the buttons. |contents_view_| owns every other view.
  std::unique_ptr<views::View> contents_view_;
  DeepScanningTopImageView* image_;
  views::ImageView* side_icon_image_;
  views::Throbber* side_icon_spinner_;
  views::Label* message_;

  views::Widget* widget_;

  bool shown_ = false;

  base::TimeTicks first_shown_timestamp_;

  // Used to show the appropriate dialog depending on the scan's status.
  DeepScanningDialogStatus dialog_status_ = DeepScanningDialogStatus::PENDING;

  // Used to animate dialog height changes.
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // The access point that caused this dialog to open. This changes what text
  // and top image are shown to the user.
  base::Optional<DeepScanAccessPoint> access_point_;

  // Indicates whether the scan being done is for files or for text. This
  // changes what text and top image are shown to the user.
  bool is_file_scan_;

  base::WeakPtrFactory<DeepScanningDialogViews> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_VIEWS_H_
