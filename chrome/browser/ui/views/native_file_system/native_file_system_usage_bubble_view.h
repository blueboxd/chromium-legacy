// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "url/origin.h"

class NativeFileSystemUsageBubbleView : public LocationBarBubbleDelegateView {
 public:
  struct Usage {
    Usage();
    ~Usage();
    Usage(Usage&&);
    Usage& operator=(Usage&&);

    std::vector<base::FilePath> readable_directories;
    std::vector<base::FilePath> writable_files;
    std::vector<base::FilePath> writable_directories;
  };
  static void ShowBubble(content::WebContents* web_contents,
                         const url::Origin& origin,
                         Usage usage);

  // Closes the showing bubble (if one exists).
  static void CloseCurrentBubble();

  // Returns the bubble if the bubble is showing. Returns null otherwise.
  static NativeFileSystemUsageBubbleView* GetBubble();

 private:
  NativeFileSystemUsageBubbleView(views::View* anchor_view,
                                  const gfx::Point& anchor_point,
                                  content::WebContents* web_contents,
                                  const url::Origin& origin,
                                  Usage usage);
  ~NativeFileSystemUsageBubbleView() override;

  // LocationBarBubbleDelegateView:
  base::string16 GetAccessibleWindowTitle() const override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldShowCloseButton() const override;
  void Init() override;
  void WindowClosing() override;
  void CloseBubble() override;
  gfx::Size CalculatePreferredSize() const override;

  void AddPathList(int details_message_id,
                   const std::vector<base::FilePath>& paths);

  // Singleton instance of the bubble. The bubble can only be shown on the
  // active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static NativeFileSystemUsageBubbleView* bubble_;

  const url::Origin origin_;
  const Usage usage_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemUsageBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_H_
