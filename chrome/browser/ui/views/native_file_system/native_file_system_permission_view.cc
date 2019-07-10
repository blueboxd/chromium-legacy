// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_permission_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_client_view.h"

NativeFileSystemPermissionView::NativeFileSystemPermissionView(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback)
    : path_(path), is_directory_(is_directory), callback_(std::move(callback)) {
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  AddChildView(native_file_system_ui_helper::CreateOriginLabel(
      is_directory ? IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_DIRECTORY_TEXT
                   : IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_FILE_TEXT,
      origin, CONTEXT_BODY_TEXT_SMALL));

  auto file_label_container = std::make_unique<views::View>();
  int indent =
      provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
  file_label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(/*vertical=*/0, indent),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  auto icon = std::make_unique<views::ImageView>();
  const gfx::VectorIcon& vector_id =
      is_directory ? vector_icons::kFolderOpenIcon
                   : vector_icons::kInsertDriveFileOutlineIcon;
  const SkColor icon_color = icon->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  icon->SetImage(gfx::CreateVectorIcon(vector_id, /*dip_size=*/18, icon_color));
  file_label_container->AddChildView(std::move(icon));
  auto file_label = std::make_unique<views::Label>(path.LossyDisplayName(),
                                                   CONTEXT_BODY_TEXT_SMALL,
                                                   STYLE_EMPHASIZED_SECONDARY);
  file_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  file_label_container->AddChildView(std::move(file_label));
  AddChildView(std::move(file_label_container));
}

NativeFileSystemPermissionView::~NativeFileSystemPermissionView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Close();
}

views::Widget* NativeFileSystemPermissionView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  auto delegate = base::WrapUnique(new NativeFileSystemPermissionView(
      origin, path, is_directory, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemPermissionView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      is_directory_ ? IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_DIRECTORY_TITLE
                    : IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_FILE_TITLE);
}

int NativeFileSystemPermissionView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 NativeFileSystemPermissionView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(
        IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_ALLOW_TEXT);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

bool NativeFileSystemPermissionView::ShouldShowCloseButton() const {
  return false;
}

bool NativeFileSystemPermissionView::Accept() {
  std::move(callback_).Run(PermissionAction::GRANTED);
  return true;
}

bool NativeFileSystemPermissionView::Cancel() {
  std::move(callback_).Run(PermissionAction::DISMISSED);
  return true;
}

gfx::Size NativeFileSystemPermissionView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemPermissionView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void ShowNativeFileSystemPermissionDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  NativeFileSystemPermissionView::ShowDialog(origin, path, is_directory,
                                             std::move(callback), web_contents);
}
