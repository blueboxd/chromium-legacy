// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_

#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/macros.h"

namespace ash {

class AssistantControllerImpl;

class AssistantViewDelegateImpl : public AssistantViewDelegate {
 public:
  AssistantViewDelegateImpl(AssistantControllerImpl* assistant_controller);
  ~AssistantViewDelegateImpl() override;

  // AssistantViewDelegate:
  const AssistantAlarmTimerModel* GetAlarmTimerModel() const override;
  const AssistantNotificationModel* GetNotificationModel() const override;
  void AddObserver(AssistantViewDelegateObserver* observer) override;
  void RemoveObserver(AssistantViewDelegateObserver* observer) override;
  void AddAlarmTimerModelObserver(
      AssistantAlarmTimerModelObserver* observer) override;
  void RemoveAlarmTimerModelObserver(
      AssistantAlarmTimerModelObserver* observer) override;
  void AddNotificationModelObserver(
      AssistantNotificationModelObserver* observer) override;
  void RemoveNotificationModelObserver(
      AssistantNotificationModelObserver* observer) override;
  void DownloadImage(const GURL& url,
                     ImageDownloader::DownloadCallback callback) override;
  ::wm::CursorManager* GetCursorManager() override;
  aura::Window* GetRootWindowForDisplayId(int64_t display_id) override;
  aura::Window* GetRootWindowForNewWindows() override;
  bool IsTabletMode() const override;
  void OnDialogPlateButtonPressed(AssistantButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;
  void OnHostViewVisibilityChanged(bool visible) override;
  void OnNotificationButtonPressed(const std::string& notification_id,
                                   int notification_button_index) override;
  void OnOptInButtonPressed() override;
  void OnProactiveSuggestionsCloseButtonPressed() override;
  void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) override;
  void OnProactiveSuggestionsViewPressed() override;
  void OnSuggestionChipPressed(const AssistantSuggestion* suggestion) override;

 private:
  AssistantControllerImpl* const assistant_controller_;
  base::ObserverList<AssistantViewDelegateObserver> view_delegate_observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantViewDelegateImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
