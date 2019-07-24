// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_view_delegate_impl.h"

#include "ash/assistant/assistant_cache_controller.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_prefs_controller.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

AssistantViewDelegateImpl::AssistantViewDelegateImpl(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {}

AssistantViewDelegateImpl::~AssistantViewDelegateImpl() = default;

const AssistantCacheModel* AssistantViewDelegateImpl::GetCacheModel() const {
  return assistant_controller_->cache_controller()->model();
}

const AssistantInteractionModel*
AssistantViewDelegateImpl::GetInteractionModel() const {
  return assistant_controller_->interaction_controller()->model();
}

const AssistantNotificationModel*
AssistantViewDelegateImpl::GetNotificationModel() const {
  return assistant_controller_->notification_controller()->model();
}

const AssistantUiModel* AssistantViewDelegateImpl::GetUiModel() const {
  return assistant_controller_->ui_controller()->model();
}

void AssistantViewDelegateImpl::AddObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}

void AssistantViewDelegateImpl::RemoveObserver(
    AssistantViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

void AssistantViewDelegateImpl::AddCacheModelObserver(
    AssistantCacheModelObserver* observer) {
  assistant_controller_->cache_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveCacheModelObserver(
    AssistantCacheModelObserver* observer) {
  assistant_controller_->cache_controller()->RemoveModelObserver(observer);
}

void AssistantViewDelegateImpl::AddInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveInteractionModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_controller_->interaction_controller()->RemoveModelObserver(
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

void AssistantViewDelegateImpl::AddUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->AddModelObserver(observer);
}

void AssistantViewDelegateImpl::RemoveUiModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_controller_->ui_controller()->RemoveModelObserver(observer);
}

void AssistantViewDelegateImpl::AddAssistantPrefsObserver(
    AssistantPrefsObserver* observer) {
  assistant_controller_->prefs_controller()->AddObserver(observer);
}

void AssistantViewDelegateImpl::RemoveAssistantPrefsObserver(
    AssistantPrefsObserver* observer) {
  assistant_controller_->prefs_controller()->RemoveObserver(observer);
}

CaptionBarDelegate* AssistantViewDelegateImpl::GetCaptionBarDelegate() {
  return assistant_controller_->ui_controller();
}

void AssistantViewDelegateImpl::DownloadImage(
    const GURL& url,
    AssistantImageDownloader::DownloadCallback callback) {
  assistant_controller_->DownloadImage(url, std::move(callback));
}

int AssistantViewDelegateImpl::GetConsentStatus() const {
  return assistant_controller_->prefs_controller()->prefs()->GetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus);
}

::wm::CursorManager* AssistantViewDelegateImpl::GetCursorManager() {
  return Shell::Get()->cursor_manager();
}

void AssistantViewDelegateImpl::GetNavigableContentsFactoryForView(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  assistant_controller_->GetNavigableContentsFactory(std::move(receiver));
}

aura::Window* AssistantViewDelegateImpl::GetRootWindowForNewWindows() {
  return Shell::Get()->GetRootWindowForNewWindows();
}

bool AssistantViewDelegateImpl::IsLaunchWithMicOpen() const {
  return assistant_controller_->prefs_controller()->prefs()->GetBoolean(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen);
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

void AssistantViewDelegateImpl::OnMiniViewPressed() {
  for (auto& observer : view_delegate_observers_)
    observer.OnMiniViewPressed();
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

void AssistantViewDelegateImpl::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnSuggestionChipPressed(suggestion);
}

void AssistantViewDelegateImpl::OpenUrlFromView(const GURL& url) {
  assistant_controller_->OpenUrl(url);
}

void AssistantViewDelegateImpl::NotifyDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  for (AssistantViewDelegateObserver& observer : view_delegate_observers_)
    observer.OnDeepLinkReceived(type, params);
}

}  // namespace ash
