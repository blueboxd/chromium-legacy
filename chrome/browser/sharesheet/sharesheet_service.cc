// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <utility>

#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetService::SharesheetService(Profile* profile)
    : sharesheet_action_cache_(std::make_unique<SharesheetActionCache>()) {}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(views::View* bubble_anchor_view) {
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

  sharesheet_service_delegate->ShowBubble(std::move(targets));

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
                                         views::View* share_action_view) {
  if (type == TargetType::kAction) {
    ShareAction* share_action =
        sharesheet_action_cache_->GetActionFromName(target_name);
    if (share_action == nullptr)
      return;

    SharesheetServiceDelegate* delegate = GetDelegate(delegate_id);
    if (delegate == nullptr)
      return;

    delegate->OnActionLaunched();
    share_action->LaunchAction(delegate, share_action_view);
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

}  // namespace sharesheet
