// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_big_user_view.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "components/account_id/account_id.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

bool IsPublicAccountUser(const LoginUserInfo& user) {
  return user.basic_user_info.type == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

bool IsChildAccountUser(const LoginUserInfo& user) {
  return user.basic_user_info.type == user_manager::USER_TYPE_CHILD;
}

// Returns true if either a or b have a value, but not both.
bool OnlyOneSet(views::View* a, views::View* b) {
  return !!a ^ !!b;
}

}  // namespace

LoginBigUserView::LoginBigUserView(
    const LoginUserInfo& user,
    const LoginAuthUserView::Callbacks& auth_user_callbacks,
    const LoginPublicAccountUserView::Callbacks& public_account_callbacks,
    const ParentAccessView::Callbacks& parent_access_callbacks)
    : NonAccessibleView(),
      auth_user_callbacks_(auth_user_callbacks),
      public_account_callbacks_(public_account_callbacks),
      parent_access_callbacks_(parent_access_callbacks) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Creates either |auth_user_| or |public_account_|. |parent_access_| cannot
  // be used as a standalone view, it is only used together with |auth_user_|.
  CreateChildView(user);
  DCHECK(!parent_access_);

  observer_.Add(Shell::Get()->wallpaper_controller());
  // Adding the observer will not run OnWallpaperBlurChanged; run it now to set
  // the initial state.
  OnWallpaperBlurChanged();
}

LoginBigUserView::~LoginBigUserView() = default;

void LoginBigUserView::CreateChildView(const LoginUserInfo& user) {
  if (IsPublicAccountUser(user))
    CreatePublicAccount(user);
  else
    CreateAuthUser(user);
}

void LoginBigUserView::UpdateForUser(const LoginUserInfo& user) {
  // Rebuild child view for the following swap case:
  // 1. Public Account -> Auth User
  // 2. Auth User      -> Public Account
  // If swap is performed when |parent_access_| is shown, both |parent_access_|
  // and |auth_user_| will be destroyed and replaced with |public_user_|.
  if (IsPublicAccountUser(user) != IsPublicAccountUser(GetCurrentUser()))
    CreateChildView(user);

  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    DCHECK(!parent_access_);
    public_account_->UpdateForUser(user);
  }
  if (auth_user_)
    auth_user_->UpdateForUser(user);
}

void LoginBigUserView::ShowParentAccessView() {
  // Can be only combined with |auth_user_|.
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  DCHECK(auth_user_);

  // Do not show parent access if LoginBigUserView does not host regular user
  // view or if ParentAccessView is already shown.
  if (!auth_user_ || parent_access_)
    return;

  DCHECK(IsChildAccountUser(auth_user_->current_user()));
  parent_access_ = new ParentAccessView(
      auth_user_->current_user().basic_user_info.account_id,
      parent_access_callbacks_, ParentAccessRequestReason::kUnlockTimeLimits);
  RemoveChildView(auth_user_);
  AddChildView(parent_access_);
  RequestFocus();
}

void LoginBigUserView::HideParentAccessView() {
  // Can be only combined with |auth_user_|.
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  DCHECK(auth_user_);

  if (!auth_user_)
    return;

  DCHECK(IsChildAccountUser(auth_user_->current_user()));
  DCHECK(parent_access_);
  delete parent_access_;
  parent_access_ = nullptr;
  AddChildView(auth_user_);
  RequestFocus();
}

const LoginUserInfo& LoginBigUserView::GetCurrentUser() const {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    DCHECK(!parent_access_);
    return public_account_->current_user();
  }
  return auth_user_->current_user();
}

LoginUserView* LoginBigUserView::GetUserView() {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    DCHECK(!parent_access_);
    return public_account_->user_view();
  }
  return auth_user_->user_view();
}

bool LoginBigUserView::IsAuthEnabled() const {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    DCHECK(!parent_access_);
    return public_account_->auth_enabled();
  }
  return auth_user_->auth_methods() != LoginAuthUserView::AUTH_NONE;
}

void LoginBigUserView::RequestFocus() {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    DCHECK(!parent_access_);
    return public_account_->RequestFocus();
  }
  if (parent_access_) {
    DCHECK(auth_user_);
    return parent_access_->RequestFocus();
  }
  return auth_user_->RequestFocus();
}

void LoginBigUserView::ChildPreferredSizeChanged(views::View* child) {
  parent()->Layout();
}

void LoginBigUserView::OnWallpaperBlurChanged() {
  if (Shell::Get()->wallpaper_controller()->IsWallpaperBlurred()) {
    SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);
    SetBackground(nullptr);
  } else {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            SkColorSetA(login_constants::kDefaultBaseColor,
                        login_constants::kNonBlurredWallpaperBackgroundAlpha),
            login_constants::kNonBlurredWallpaperBackgroundRadiusDp)));
  }
}

void LoginBigUserView::CreateAuthUser(const LoginUserInfo& user) {
  DCHECK(!IsPublicAccountUser(user));
  DCHECK(!auth_user_);
  DCHECK(!parent_access_);

  auth_user_ = new LoginAuthUserView(user, auth_user_callbacks_);
  delete public_account_;
  public_account_ = nullptr;
  AddChildView(auth_user_);
}

void LoginBigUserView::CreatePublicAccount(const LoginUserInfo& user) {
  DCHECK(IsPublicAccountUser(user));
  DCHECK(!public_account_);

  public_account_ =
      new LoginPublicAccountUserView(user, public_account_callbacks_);
  if (parent_access_) {
    delete parent_access_;
    parent_access_ = nullptr;
  }
  delete auth_user_;
  auth_user_ = nullptr;
  AddChildView(public_account_);
}

}  // namespace ash
