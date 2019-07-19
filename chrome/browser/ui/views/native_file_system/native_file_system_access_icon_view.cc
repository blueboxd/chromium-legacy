// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_access_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
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

  // If icon isn't visible, a bubble shouldn't be shown either. Close it if
  // it was still open.
  if (!GetVisible())
    NativeFileSystemUsageBubbleView::CloseCurrentBubble();

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

  ChromeNativeFileSystemPermissionContext::GetPermissionGrantsFromUIThread(
      GetWebContents()->GetBrowserContext(), origin,
      GetWebContents()->GetMainFrame()->GetProcess()->GetID(),
      GetWebContents()->GetMainFrame()->GetRoutingID(),
      base::BindOnce(
          [](int frame_tree_node_id, const url::Origin& origin,
             ChromeNativeFileSystemPermissionContext::Grants grants) {
            auto* web_contents =
                content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
            if (!web_contents ||
                url::Origin::Create(web_contents->GetLastCommittedURL()) !=
                    origin) {
              // If web-contents has navigated to a different origin while we
              // were looking up current usage, don't show the dialog to avoid
              // showing the dialog for the wrong origin.
              return;
            }

            NativeFileSystemUsageBubbleView::Usage usage;
            usage.readable_directories =
                web_contents->GetNativeFileSystemDirectoryHandles();
            usage.writable_files = std::move(grants.file_write_grants);
            usage.writable_directories =
                std::move(grants.directory_write_grants);

            if (usage.readable_directories.empty() &&
                usage.writable_files.empty() &&
                usage.writable_directories.empty()) {
              // Async looking up of usage might result in there no longer being
              // usage by the time we get here, in that case just abort and
              // don't show the bubble.
              return;
            }

            NativeFileSystemUsageBubbleView::ShowBubble(web_contents, origin,
                                                        std::move(usage));
          },
          GetWebContents()->GetMainFrame()->GetFrameTreeNodeId(), origin));
}

const gfx::VectorIcon& NativeFileSystemAccessIconView::GetVectorIcon() const {
  return has_write_access_ ? kSaveOriginalFileIcon
                           : vector_icons::kInsertDriveFileOutlineIcon;
}
