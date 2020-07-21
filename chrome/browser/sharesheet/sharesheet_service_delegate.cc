// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/ui/views/sharesheet_bubble_view.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegate::SharesheetServiceDelegate(
    uint32_t id,
    views::View* bubble_anchor_view,
    SharesheetService* sharesheet_service)
    : id_(id),
      sharesheet_bubble_view_(
          std::make_unique<SharesheetBubbleView>(bubble_anchor_view, this)),
      sharesheet_service_(sharesheet_service) {}

SharesheetServiceDelegate::~SharesheetServiceDelegate() = default;

void SharesheetServiceDelegate::ShowBubble(std::vector<TargetInfo> targets) {
  sharesheet_bubble_view_->ShowBubble(std::move(targets));
}

void SharesheetServiceDelegate::OnBubbleClosed(
    const base::string16& active_action) {
  sharesheet_bubble_view_.release();
  sharesheet_service_->OnBubbleClosed(id_, active_action);
}

void SharesheetServiceDelegate::OnTargetSelected(
    const base::string16& target_name,
    const TargetType type,
    views::View* share_action_view) {
  sharesheet_service_->OnTargetSelected(id_, target_name, type,
                                        share_action_view);
}

void SharesheetServiceDelegate::OnActionLaunched() {
  sharesheet_bubble_view_->ShowActionView();
}

uint32_t SharesheetServiceDelegate::GetId() {
  return id_;
}

void SharesheetServiceDelegate::ShareActionCompleted() {
  sharesheet_bubble_view_->CloseBubble();
}

}  // namespace sharesheet
