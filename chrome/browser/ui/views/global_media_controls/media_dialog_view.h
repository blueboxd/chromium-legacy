// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_

#include "base/optional.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_controller.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_controller_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

class MediaNotificationListView;

// Dialog that shows media controls that control the active media session.
class MediaDialogView : public views::BubbleDialogDelegateView,
                        public MediaDialogControllerDelegate {
 public:
  static void ShowDialog(views::View* anchor_view,
                         service_manager::Connector* connector);
  static void HideDialog();
  static bool IsShowing();

  // MediaDialogControllerDelegate implementation.
  void ShowMediaSession(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaSession(const std::string& id) override;

  // views::DialogDelegate implementation.
  int GetDialogButtons() const override;
  bool Close() override;

  // views::View implementation.
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  explicit MediaDialogView(views::View* anchor_view,
                           service_manager::Connector* connector);
  ~MediaDialogView() override;

  static MediaDialogView* instance_;

  // views::BubbleDialogDelegateView implementation.
  void Init() override;
  void WindowClosing() override;

  MediaDialogController controller_;

  MediaNotificationListView* const active_sessions_view_;

  DISALLOW_COPY_AND_ASSIGN(MediaDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
