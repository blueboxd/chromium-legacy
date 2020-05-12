// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr int icon_size_dip = 48;

}  // namespace

MoveToAccountStoreBubbleController::MoveToAccountStoreBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::AUTOMATIC_MOVE_TO_ACCOUNT_STORE) {}

MoveToAccountStoreBubbleController::~MoveToAccountStoreBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  if (!interaction_reported_)
    OnBubbleClosing();
}

base::string16 MoveToAccountStoreBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_TITLE);
}

void MoveToAccountStoreBubbleController::AcceptMove() {
  return delegate_->MovePasswordToAccountStore();
}

gfx::Image MoveToAccountStoreBubbleController::GetProfileIcon() {
  if (!GetProfile())
    return gfx::Image();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  if (!identity_manager)
    return gfx::Image();
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kNotRequired));
  DCHECK(primary_account_info.has_value());
  return profiles::GetSizedAvatarIcon(primary_account_info->account_image,
                                      /*is_rectangle=*/true, icon_size_dip,
                                      icon_size_dip, profiles::SHAPE_CIRCLE);
}

void MoveToAccountStoreBubbleController::ReportInteractions() {}
