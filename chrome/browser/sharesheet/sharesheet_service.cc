// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetService::SharesheetService(Profile* profile)
    : sharesheet_action_cache_(std::make_unique<SharesheetActionCache>()),
      app_service_proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)) {
  DCHECK(app_service_proxy_);
}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(views::View* bubble_anchor_view,
                                   apps::mojom::IntentPtr intent) {
  DCHECK(intent->action == apps_util::kIntentActionSend ||
         intent->action == apps_util::kIntentActionSendMultiple);
  auto sharesheet_service_delegate =
      std::make_unique<SharesheetServiceDelegate>(
          delegate_counter_++, std::move(bubble_anchor_view), this);

  std::vector<TargetInfo> targets;
  auto& actions = sharesheet_action_cache_->GetShareActions();
  auto iter = actions.begin();
  while (iter != actions.end()) {
    targets.emplace(targets.begin(), TargetType::kAction,
                    (*iter)->GetActionIcon(), (*iter)->GetActionName(),
                    (*iter)->GetActionName());
    ++iter;
  }

  std::vector<apps::AppIdAndActivityName> app_id_and_activities =
      app_service_proxy_->GetAppsForIntent(intent);
  for (const auto& app_id_and_activity : app_id_and_activities) {
    // TODO(1097623) : Load Real Icons.
    targets.emplace(targets.begin(), TargetType::kApp, gfx::Image(),
                    base::UTF8ToUTF16(app_id_and_activity.app_id),
                    base::UTF8ToUTF16(app_id_and_activity.activity_name));
  }

  sharesheet_service_delegate->ShowBubble(std::move(targets),
                                          std::move(intent));

  active_delegates_.push_back(std::move(sharesheet_service_delegate));
}

// Cleanup delegate when bubble closes.
void SharesheetService::OnBubbleClosed(uint32_t id,
                                       const base::string16& active_action) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetId() == id) {
      if (!active_action.empty()) {
        ShareAction* share_action =
            sharesheet_action_cache_->GetActionFromName(active_action);
        if (share_action != nullptr)
          share_action->OnClosing(iter->get());
      }
      active_delegates_.erase(iter);
      break;
    }
    ++iter;
  }
}

void SharesheetService::OnTargetSelected(uint32_t delegate_id,
                                         const base::string16& target_name,
                                         const TargetType type,
                                         apps::mojom::IntentPtr intent,
                                         views::View* share_action_view) {
  SharesheetServiceDelegate* delegate = GetDelegate(delegate_id);
  if (delegate == nullptr)
    return;

  if (type == TargetType::kAction) {
    ShareAction* share_action =
        sharesheet_action_cache_->GetActionFromName(target_name);
    if (share_action == nullptr)
      return;
    delegate->OnActionLaunched();
    share_action->LaunchAction(delegate, share_action_view, std::move(intent));
  } else if (type == TargetType::kApp) {
    auto launch_source = apps::mojom::LaunchSource::kFromSharesheet;
    app_service_proxy_->LaunchAppWithIntent(
        base::UTF16ToUTF8(target_name),
        apps::GetEventFlags(
            apps::mojom::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW,
            /*prefer_container=*/true),
        std::move(intent), launch_source, display::kDefaultDisplayId);
  }
}

SharesheetServiceDelegate* SharesheetService::GetDelegate(
    uint32_t delegate_id) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetId() == delegate_id) {
      return iter->get();
    }
    ++iter;
  }
  return nullptr;
}

bool SharesheetService::HasShareTargets(apps::mojom::IntentPtr intent) {
  auto& actions = sharesheet_action_cache_->GetShareActions();
  std::vector<apps::AppIdAndActivityName> app_id_and_activities =
      app_service_proxy_->GetAppsForIntent(intent);

  return !actions.empty() || !app_id_and_activities.empty();
}

}  // namespace sharesheet
