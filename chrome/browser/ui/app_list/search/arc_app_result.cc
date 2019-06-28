// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"

namespace {
const char kArcAppPrefix[] = "arc://";
}

namespace app_list {

ArcAppResult::ArcAppResult(Profile* profile,
                           const std::string& app_id,
                           AppListControllerDelegate* controller,
                           bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  std::string id = kArcAppPrefix;
  id += app_id;
  set_id(id);
}

ArcAppResult::~ArcAppResult() {}

void ArcAppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, GetContextMenuAppLaunchInteraction());
}

void ArcAppResult::Open(int event_flags) {
  Launch(event_flags, GetAppLaunchInteraction());
}

void ArcAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcAppContextMenu>(this, profile(), app_id(),
                                                      controller());
  context_menu_->GetMenuModel(std::move(callback));
}

SearchResultType ArcAppResult::GetSearchResultType() const {
  return PLAY_STORE_APP;
}

AppContextMenu* ArcAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

void ArcAppResult::Launch(int event_flags,
                          arc::UserInteractionType interaction) {
  arc::LaunchApp(profile(), app_id(), event_flags, interaction,
                 controller()->GetAppListDisplayId());
}

arc::UserInteractionType ArcAppResult::GetAppLaunchInteraction() {
  return display_type() == ash::SearchResultDisplayType::kRecommendation
             ? arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP
             : arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
}

arc::UserInteractionType ArcAppResult::GetContextMenuAppLaunchInteraction() {
  return display_type() == ash::SearchResultDisplayType::kRecommendation
             ? arc::UserInteractionType::
                   APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP_CONTEXT_MENU
             : arc::UserInteractionType::
                   APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
}

}  // namespace app_list
