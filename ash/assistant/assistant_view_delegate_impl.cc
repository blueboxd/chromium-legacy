// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_view_delegate_impl.h"

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

AssistantViewDelegateImpl::AssistantViewDelegateImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AssistantViewDelegateImpl::~AssistantViewDelegateImpl() = default;

const AssistantAlarmTimerModel* AssistantViewDelegateImpl::GetAlarmTimerModel()
    const {
  return assistant_controller_->alarm_timer_controller()->model();
}

const AssistantNotificationModel*
AssistantViewDelegateImpl::GetNotificationModel() const {
  return assistant_controller_->notification_controller()->model();
}

void AssistantViewDelegateImpl::AddObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}

void AssistantViewDelegateImpl::RemoveObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

void AssistantViewDelegateImpl::AddAlarmTimerModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  assistant_controller_->alarm_timer_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveAlarmTimerModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  assistant_controller_->alarm_timer_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::AddNotificationModelObserver(
    AssistantNotificationModelObserver* observer) {
  assistant_controller_->notification_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveNotificationModelObserver(
    AssistantNotificationModelObserver* observer) {
  assistant_controller_->notification_controller()->RemoveModelObserver(
      observer);
}

void AssistantViewDelegateImpl::DownloadImage(
    const GURL& url,
    ImageDownloader::DownloadCallback callback) {
  assistant_controller_->DownloadImage(url, std::move(callback));
}

::wm::CursorManager* AssistantViewDelegateImpl::GetCursorManager() {
  return Shell::Get()->cursor_manager();
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return Shell::Get()->GetRootWindowForDisplayId(display_id);
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForNewWindows() {
  return Shell::Get()->GetRootWindowForNewWindows();
}

bool AssistantViewDelegateImpl::IsTabletMode() const {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void AssistantViewDelegateImpl::OnDialogPlateButtonPressed(
    AssistantButtonId id) {
  for (auto& observer : view_delegate_observers_)
    observer.OnDialogPlateButtonPressed(id);
}

void AssistantViewDelegateImpl::OnDialogPlateContentsCommitted(
    const std::string& text) {
  for (auto& observer : view_delegate_observers_)
    observer.OnDialogPlateContentsCommitted(text);
}

void AssistantViewDelegateImpl::OnHostViewVisibilityChanged(bool visible) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnHostViewVisibilityChanged(visible);
}

void AssistantViewDelegateImpl::OnNotificationButtonPressed(
    const std::string& notification_id,
    int notification_button_index) {
  assistant_controller_->notification_controller()->OnNotificationClicked(
      notification_id, notification_button_index, /*reply=*/base::nullopt);
}

void AssistantViewDelegateImpl::OnOptInButtonPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnOptInButtonPressed();
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsCloseButtonPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsCloseButtonPressed();
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsViewHoverChanged(
    bool is_hovering) {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsViewHoverChanged(is_hovering);
}

void AssistantViewDelegateImpl::OnProactiveSuggestionsViewPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnProactiveSuggestionsViewPressed();
}

void AssistantViewDelegateImpl::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnSuggestionChipPressed(suggestion);
}

}  // namespace ash
