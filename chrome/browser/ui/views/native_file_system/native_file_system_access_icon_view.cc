// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_access_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

NativeFileSystemAccessIconView::NativeFileSystemAccessIconView(
    Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate) {
  SetVisible(false);
}

views::BubbleDialogDelegateView* NativeFileSystemAccessIconView::GetBubble()
    const {
  return NativeFileSystemUsageBubbleView::GetBubble();
}

bool NativeFileSystemAccessIconView::Update() {
  const bool was_visible = GetVisible();
  const bool had_write_access = has_write_access_;

  SetVisible(GetWebContents() &&
             (GetWebContents()->HasWritableNativeFileSystemHandles() ||
              GetWebContents()->HasNativeFileSystemDirectoryHandles()));

  has_write_access_ = GetWebContents() &&
                      GetWebContents()->HasWritableNativeFileSystemHandles();
  if (has_write_access_ != had_write_access)
    UpdateIconImage();
  return GetVisible() != was_visible || had_write_access != has_write_access_;
}

base::string16
NativeFileSystemAccessIconView::GetTextForTooltipAndAccessibleName() const {
  return has_write_access_
             ? l10n_util::GetStringUTF16(
                   IDS_NATIVE_FILE_SYSTEM_WRITE_USAGE_TOOLTIP)
             : l10n_util::GetStringUTF16(
                   IDS_NATIVE_FILE_SYSTEM_DIRECTORY_USAGE_TOOLTIP);
}

void NativeFileSystemAccessIconView::OnExecuting(ExecuteSource execute_source) {
  url::Origin origin =
      url::Origin::Create(GetWebContents()->GetLastCommittedURL());

  // TODO(https://crbug.com/979684): Display actual files and directories,
  // rather than these random hard coded values.
  NativeFileSystemUsageBubbleView::Usage usage;
  usage.readable_directories.emplace_back(FILE_PATH_LITERAL("My Fonts"));
  usage.writable_files.emplace_back(FILE_PATH_LITERAL("/foo/bar/demo.txt"));
  usage.writable_files.emplace_back(FILE_PATH_LITERAL("README.md"));
  usage.writable_directories.emplace_back(FILE_PATH_LITERAL("My Project"));
  NativeFileSystemUsageBubbleView::ShowBubble(GetWebContents(), origin,
                                              std::move(usage));
}

const gfx::VectorIcon& NativeFileSystemAccessIconView::GetVectorIcon() const {
  return has_write_access_ ? kSaveOriginalFileIcon
                           : vector_icons::kInsertDriveFileOutlineIcon;
}
