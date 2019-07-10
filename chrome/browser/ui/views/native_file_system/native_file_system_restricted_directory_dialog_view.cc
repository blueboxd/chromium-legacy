// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_restricted_directory_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_client_view.h"

NativeFileSystemRestrictedDirectoryDialogView::
    ~NativeFileSystemRestrictedDirectoryDialogView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Accept();
}

// static
views::Widget* NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceClosure callback,
    content::WebContents* web_contents) {
  auto delegate =
      base::WrapUnique(new NativeFileSystemRestrictedDirectoryDialogView(
          origin, path, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemRestrictedDirectoryDialogView::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TITLE);
}

int NativeFileSystemRestrictedDirectoryDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16
NativeFileSystemRestrictedDirectoryDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_BUTTON);
}

bool NativeFileSystemRestrictedDirectoryDialogView::ShouldShowCloseButton()
    const {
  return false;
}

bool NativeFileSystemRestrictedDirectoryDialogView::Accept() {
  std::move(callback_).Run();
  return true;
}

bool NativeFileSystemRestrictedDirectoryDialogView::Cancel() {
  std::move(callback_).Run();
  return true;
}

gfx::Size
NativeFileSystemRestrictedDirectoryDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemRestrictedDirectoryDialogView::GetModalType()
    const {
  return ui::MODAL_TYPE_CHILD;
}

NativeFileSystemRestrictedDirectoryDialogView::
    NativeFileSystemRestrictedDirectoryDialogView(const url::Origin& origin,
                                                  const base::FilePath& path,
                                                  base::OnceClosure callback)
    : callback_(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TEXT,
          url_formatter::FormatOriginForSecurityDisplay(
              origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)),
      CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label.release());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
}

void ShowNativeFileSystemRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceClosure callback,
    content::WebContents* web_contents) {
  NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
      origin, path, std::move(callback), web_contents);
}
