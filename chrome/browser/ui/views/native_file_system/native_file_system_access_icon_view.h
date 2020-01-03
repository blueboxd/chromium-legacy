// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ACCESS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ACCESS_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

// Page action icon indicating if the current page is using the native file
// system API. Shows different icons for read access to directories and write
// access to files or directories.
class NativeFileSystemAccessIconView : public PageActionIconView {
 public:
  NativeFileSystemAccessIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  void UpdateImpl() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  void OnExecuting(ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const char* GetClassName() const override;

 private:
  bool has_write_access_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemAccessIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ACCESS_ICON_VIEW_H_
