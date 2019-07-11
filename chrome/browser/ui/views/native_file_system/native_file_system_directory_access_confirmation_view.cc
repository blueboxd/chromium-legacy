// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_directory_access_confirmation_view.h"

#include "base/files/file_path.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_client_view.h"

NativeFileSystemDirectoryAccessConfirmationView::
    ~NativeFileSystemDirectoryAccessConfirmationView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Close();
}

// static
views::Widget* NativeFileSystemDirectoryAccessConfirmationView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  auto delegate =
      base::WrapUnique(new NativeFileSystemDirectoryAccessConfirmationView(
          origin, path, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemDirectoryAccessConfirmationView::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_TITLE);
}

int NativeFileSystemDirectoryAccessConfirmationView::GetDefaultDialogButton()
    const {
  return ui::DIALOG_BUTTON_NONE;
}

base::string16
NativeFileSystemDirectoryAccessConfirmationView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

bool NativeFileSystemDirectoryAccessConfirmationView::ShouldShowCloseButton()
    const {
  return false;
}

bool NativeFileSystemDirectoryAccessConfirmationView::Accept() {
  std::move(callback_).Run(PermissionAction::GRANTED);
  return true;
}

bool NativeFileSystemDirectoryAccessConfirmationView::Cancel() {
  std::move(callback_).Run(PermissionAction::DISMISSED);
  return true;
}

gfx::Size
NativeFileSystemDirectoryAccessConfirmationView::CalculatePreferredSize()
    const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemDirectoryAccessConfirmationView::GetModalType()
    const {
  return ui::MODAL_TYPE_CHILD;
}

NativeFileSystemDirectoryAccessConfirmationView::
    NativeFileSystemDirectoryAccessConfirmationView(
        const url::Origin& origin,
        const base::FilePath& path,
        base::OnceCallback<void(PermissionAction result)> callback)
    : callback_(std::move(callback)) {
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  base::string16 formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  size_t offset;
  auto label = std::make_unique<views::StyledLabel>(
      l10n_util::GetStringFUTF16(
          IDS_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_TEXT,
          formatted_origin, &offset),
      nullptr);
  label->SetTextContext(CONTEXT_BODY_TEXT_SMALL);
  label->SetDefaultTextStyle(STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::StyledLabel::RangeStyleInfo origin_style;
  origin_style.text_style = STYLE_EMPHASIZED_SECONDARY;
  label->AddStyleRange(gfx::Range(offset, offset + formatted_origin.length()),
                       origin_style);

  AddChildView(std::move(label));

  auto file_label_container = std::make_unique<views::View>();
  int indent =
      provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
  file_label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(/*vertical=*/0, indent),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  auto icon = std::make_unique<views::ImageView>();
  const SkColor icon_color = icon->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  icon->SetImage(gfx::CreateVectorIcon(vector_icons::kFolderOpenIcon,
                                       /*dip_size=*/18, icon_color));
  file_label_container->AddChildView(std::move(icon));
  auto file_label = std::make_unique<views::Label>(path.LossyDisplayName(),
                                                   CONTEXT_BODY_TEXT_SMALL,
                                                   STYLE_EMPHASIZED_SECONDARY);
  file_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  file_label_container->AddChildView(std::move(file_label));
  AddChildView(std::move(file_label_container));
}

void ShowNativeFileSystemDirectoryAccessConfirmationDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  NativeFileSystemDirectoryAccessConfirmationView::ShowDialog(
      origin, path, std::move(callback), web_contents);
}
