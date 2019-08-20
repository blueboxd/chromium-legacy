// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"

namespace {

constexpr base::TimeDelta kEmailExpansionDuration =
    base::TimeDelta::FromSeconds(3);

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    return nullptr;
  }
  return entry;
}

}  // namespace

AvatarToolbarButton::AvatarToolbarButton(Browser* browser)
    : ToolbarButton(nullptr),
      browser_(browser),
      profile_(browser_->profile()),
#if !defined(OS_CHROMEOS)
      error_controller_(this, profile_),
#endif  // !defined(OS_CHROMEOS)
      browser_list_observer_(this),
      profile_observer_(this),
      identity_manager_observer_(this),
      weak_ptr_factory_(this) {
  if (IsIncognito())
    browser_list_observer_.Add(BrowserList::GetInstance());

  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  if (profile_->IsRegularProfile()) {
    identity_manager_observer_.Add(
        IdentityManagerFactory::GetForProfile(profile_));
  }

  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  button_controller()->set_notify_action(
      views::ButtonController::NOTIFY_ON_PRESS);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON);

  set_tag(IDC_SHOW_AVATAR_MENU);

  // The avatar should not flip with RTL UI. This does not affect text rendering
  // and LabelButton image/label placement is still flipped like usual.
  EnableCanvasFlippingForRTLUI(false);

  Init();

#if defined(OS_CHROMEOS)
  // On CrOS this button should only show as badging for Incognito and Guest
  // sessions. It's only enabled for Incognito where a menu is available for
  // closing all Incognito windows.
  DCHECK(!profile_->IsRegularProfile());
  SetEnabled(IsIncognito());
#endif  // !defined(OS_CHROMEOS)

  if (base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)) {
    // For consistency with identity representation, we need to have the avatar
    // on the left and the (potential) user-name / email on the right.
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  // Set initial text and tooltip. UpdateIcon() needs to be called from the
  // outside as GetThemeProvider() is not available until the button is added to
  // ToolbarView's hierarchy.
  UpdateText();

  md_observer_.Add(ui::MaterialDesignController::GetInstance());
}

AvatarToolbarButton::~AvatarToolbarButton() {}

void AvatarToolbarButton::UpdateIcon() {
  gfx::Image gaia_image = GetGaiaImage();
  SetImage(views::Button::STATE_NORMAL, GetAvatarIcon(gaia_image));

  // TODO(crbug.com/983182): Move this logic to OnExtendedAccountInfoUpdated()
  // once GetGaiaImage() is updated to use GetUnconsentedPrimaryAccountInfo().
  if (waiting_for_image_to_show_user_email_ && !gaia_image.IsEmpty()) {
    waiting_for_image_to_show_user_email_ = false;
    ExpandToShowEmail();
  }
}

void AvatarToolbarButton::UpdateText() {
  base::Optional<SkColor> color;
  base::string16 text;

  const SyncState sync_state = GetSyncState();

  if (IsIncognito() && GetThemeProvider()) {
    // Note that this chip does not have a highlight color.
    const SkColor text_color = GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
    SetEnabledTextColors(text_color);
  }

  if (IsIncognito()) {
    int incognito_window_count =
        BrowserList::GetIncognitoSessionsActiveForProfile(profile_);
    SetAccessibleName(l10n_util::GetPluralStringFUTF16(
        IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE, incognito_window_count));
    text = l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_INCOGNITO,
                                            incognito_window_count);
  } else if (!suppress_avatar_button_state_ &&
             sync_state == SyncState::kError) {
    color = AdjustHighlightColorForContrast(
        GetThemeProvider(), gfx::kGoogleRed300, gfx::kGoogleRed600,
        gfx::kGoogleRed050, gfx::kGoogleRed900);
    text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
  } else if (!suppress_avatar_button_state_ &&
             sync_state == SyncState::kPaused) {
    color = AdjustHighlightColorForContrast(
        GetThemeProvider(), gfx::kGoogleBlue300, gfx::kGoogleBlue600,
        gfx::kGoogleBlue050, gfx::kGoogleBlue900);

    text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
  } else if (user_email_to_show_.has_value() &&
             !waiting_for_image_to_show_user_email_) {
    text = base::UTF8ToUTF16(*user_email_to_show_);
    if (GetThemeProvider()) {
      const SkColor text_color =
          GetThemeProvider()->GetColor(ThemeProperties::COLOR_TAB_TEXT);
      SetEnabledTextColors(text_color);
    }
  }

  SetInsets();
  SetHighlightColor(color);
  SetText(text);
  SetTooltipText(GetAvatarTooltipText());
}

void AvatarToolbarButton::SetSuppressAvatarButtonState(
    bool suppress_avatar_button_state) {
  DCHECK(!IsIncognito());
  suppress_avatar_button_state_ = suppress_avatar_button_state;
  UpdateText();
}

void AvatarToolbarButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  // TODO(bsep): Other toolbar buttons have ToolbarView as a listener and let it
  // call ExecuteCommandWithDisposition on their behalf. Unfortunately, it's not
  // possible to plumb IsKeyEvent through, so this has to be a special case.
  browser_->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      event.IsKeyEvent());
}

void AvatarToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::AddedToWidget() {
  UpdateText();
}

void AvatarToolbarButton::OnAvatarErrorChanged() {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnBrowserAdded(Browser* browser) {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnBrowserRemoved(Browser* browser) {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnProfileAdded(const base::FilePath& profile_path) {
  // Adding any profile changes the profile count, we might go from showing a
  // generic avatar button to profile pictures here. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  // Removing a profile changes the profile count, we might go from showing
  // per-profile icons back to a generic avatar icon. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  UpdateText();
}

void AvatarToolbarButton::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  if (!unconsented_primary_account_info.IsEmpty() &&
      base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)) {
    user_email_to_show_ = unconsented_primary_account_info.email;
    // If we already have a gaia image, the pill will be immediately
    // displayed by UpdateIcon().
    waiting_for_image_to_show_user_email_ = true;
    UpdateIcon();
  }
}

void AvatarToolbarButton::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateIcon();
}

void AvatarToolbarButton::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateIcon();
}

void AvatarToolbarButton::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  UpdateIcon();
}

void AvatarToolbarButton::OnTouchUiChanged() {
  SetInsets();
  PreferredSizeChanged();
}

void AvatarToolbarButton::ExpandToShowEmail() {
  DCHECK(user_email_to_show_.has_value());
  DCHECK(!waiting_for_image_to_show_user_email_);

  UpdateText();

  // Hide the pill after a while.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButton::ResetUserEmailToShow,
                     weak_ptr_factory_.GetWeakPtr()),
      kEmailExpansionDuration);
}

void AvatarToolbarButton::ResetUserEmailToShow() {
  DCHECK(user_email_to_show_.has_value());
  user_email_to_show_ = base::nullopt;

  // Update the text to the pre-shown state. This also makes sure that we now
  // reflect changes that happened while the identity pill was shown.
  UpdateText();
}

bool AvatarToolbarButton::IsIncognito() const {
  return profile_->IsIncognitoProfile();
}

bool AvatarToolbarButton::ShouldShowGenericIcon() const {
  // This function should only be used for regular profiles. Guest and Incognito
  // sessions should be handled separately and never call this function.
  DCHECK(profile_->IsRegularProfile());
#if !defined(OS_CHROMEOS)
  if (!signin_ui_util::GetAccountsForDicePromos(profile_).empty())
    return false;
#endif  // !defined(OS_CHROMEOS)

  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return true;
  }

  // If the profile is using the placeholder avatar, fall back on the themeable
  // vector icon instead.
  if (entry->GetAvatarIconIndex() == profiles::GetPlaceholderAvatarIndex())
    return true;

  return entry->GetAvatarIconIndex() == 0 &&
         g_browser_process->profile_manager()
                 ->GetProfileAttributesStorage()
                 .GetNumberOfProfiles() == 1 &&
         !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount();
}

base::string16 AvatarToolbarButton::GetAvatarTooltipText() const {
  if (IsIncognito())
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);

  if (profile_->IsGuestSession())
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);

  if (ShouldShowGenericIcon())
    return l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL);

  if (user_email_to_show_.has_value() && !waiting_for_image_to_show_user_email_)
    return base::UTF8ToUTF16(*user_email_to_show_);

  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile_->GetPath());
  switch (GetSyncState()) {
    case SyncState::kNormal:
      return profile_name;
    case SyncState::kPaused:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED_TOOLTIP,
                                        profile_name);
    case SyncState::kError:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP,
                                        profile_name);
  }

  NOTREACHED();
  return base::string16();
}

gfx::ImageSkia AvatarToolbarButton::GetAvatarIcon(
    const gfx::Image& gaia_image) const {
  // Note that the non-touchable icon size is larger than the default to
  // make the avatar icon easier to read.
  const int icon_size =
      ui::MaterialDesignController::touch_ui() ? kDefaultTouchableIconSize : 20;

  SkColor icon_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);

  if (IsIncognito())
    return gfx::CreateVectorIcon(kIncognitoIcon, icon_size, icon_color);

  if (profile_->IsGuestSession())
    return gfx::CreateVectorIcon(kUserMenuGuestIcon, icon_size, icon_color);

  gfx::Image avatar_icon;
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!ShouldShowGenericIcon() && entry) {
    if (!gaia_image.IsEmpty()) {
      avatar_icon = gaia_image;
    } else {
      avatar_icon = entry->GetAvatarIcon();
    }
  }

  if (!avatar_icon.IsEmpty()) {
    return profiles::GetSizedAvatarIcon(avatar_icon, true, icon_size, icon_size,
                                        profiles::SHAPE_CIRCLE)
        .AsImageSkia();
  }

  return gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size, icon_color);
}

gfx::Image AvatarToolbarButton::GetGaiaImage() const {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return gfx::Image();
  }

  // If there is a GAIA image available, try to use that.
  if (entry->IsUsingGAIAPicture()) {
    // The GetGAIAPicture API call will trigger an async image load from disk if
    // it has not been loaded.
    const gfx::Image* gaia_image = entry->GetGAIAPicture();

    if (gaia_image)
      return *gaia_image;
    return gfx::Image();
  }

#if !defined(OS_CHROMEOS)
  // Try to show the first account icon of the sync promo when the following
  // conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't signed in
  //  - the profile icon wasn't explicitly changed
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
      !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount() &&
      entry->IsUsingDefaultAvatar()) {
    // TODO(crbug.com/983182): Update to use GetUnconsentedPrimaryAccountInfo()
    // and maybe move this whole logic to OnExtendedAccountInfoUpdated() which
    // is the only callback where the image can change (and cache the resulting
    // image as and cache it as |unconsented_primary_account_gaia_image_|).
    std::vector<AccountInfo> promo_accounts =
        signin_ui_util::GetAccountsForDicePromos(profile_);
    if (!promo_accounts.empty()) {
      return promo_accounts.front().account_image;
    }
  }
#endif  // !defined(OS_CHROMEOS)
  return gfx::Image();
}

AvatarToolbarButton::SyncState AvatarToolbarButton::GetSyncState() const {
#if !defined(OS_CHROMEOS)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager && identity_manager->HasPrimaryAccount() &&
      profile_->IsSyncAllowed() && error_controller_.HasAvatarError()) {
    // When DICE is enabled and the error is an auth error, the sync-paused
    // icon is shown.
    int unused;
    const bool should_show_sync_paused_ui =
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
        sync_ui_util::GetMessagesForAvatarSyncError(
            profile_, &unused, &unused) == sync_ui_util::AUTH_ERROR;
    return should_show_sync_paused_ui ? SyncState::kPaused : SyncState::kError;
  }
#endif  // !defined(OS_CHROMEOS)
  return SyncState::kNormal;
}

void AvatarToolbarButton::SetInsets() {
  // In non-touch mode we use a larger-than-normal icon size for avatars as 16dp
  // is hard to read for user avatars, so we need to set corresponding insets.
  gfx::Insets layout_insets(ui::MaterialDesignController::touch_ui() ? 0 : -2);

  SetLayoutInsetDelta(layout_insets);
}
