// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <utility>

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetService::SharesheetService(Profile* profile)
    : sharesheet_action_cache_(std::make_unique<SharesheetActionCache>()) {}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(views::View* bubble_anchor_view) {
  auto sharesheet_service_delegate =
      std::make_unique<SharesheetServiceDelegate>(
          delegate_counter_++, std::move(bubble_anchor_view), this);

  sharesheet_service_delegate->ShowBubble();

  active_delegates_.push_back(std::move(sharesheet_service_delegate));
}

// Cleanup delegate when bubble closes.
void SharesheetService::OnBubbleClosed(uint32_t id) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetId() == id) {
      active_delegates_.erase(iter);
      break;
    }
    ++iter;
  }
}

}  // namespace sharesheet
