// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

// Helpers --------------------------------------------------------------------

constexpr float kShortcutIconToImageRatio = 9.0 / 16.0;

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  CHECK(g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath(), &entry));
  return entry;
}

void NavigateToGoogleAccountPage(Profile* profile, const std::string& email) {
  // Create a URL so that the account chooser is shown if the account with
  // |email| is not signed into the web. Include a UTM parameter to signal the
  // source of the navigation.
  GURL google_account = net::AppendQueryParameter(
      GURL(chrome::kGoogleAccountURL), "utm_source", "chrome-profile-chooser");

  GURL url(chrome::kGoogleAccountChooserURL);
  url = net::AppendQueryParameter(url, "Email", email);
  url = net::AppendQueryParameter(url, "continue", google_account.spec());

  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

// Returns the number of browsers associated with |profile|.
// Note: For regular profiles this includes incognito sessions.
int CountBrowsersFor(Profile* profile) {
  int browser_count = chrome::GetBrowserCount(profile);
  if (!profile->IsOffTheRecord() && profile->HasOffTheRecordProfile())
    browser_count += chrome::GetBrowserCount(profile->GetOffTheRecordProfile());
  return browser_count;
}

SkColor GetSyncErrorBackgroundColor(bool sync_paused) {
  constexpr int kAlpha = 16;
  ui::NativeTheme::ColorId base_color_id =
      sync_paused ? ui::NativeTheme::kColorId_ProminentButtonColor
                  : ui::NativeTheme::kColorId_AlertSeverityHigh;
  SkColor base_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(base_color_id);
  return SkColorSetA(base_color, kAlpha);
}

bool IsSyncPaused(Profile* profile) {
  int unused;
  return sync_ui_util::GetMessagesForAvatarSyncError(
             profile, &unused, &unused) == sync_ui_util::AUTH_ERROR;
}

}  // namespace

// ProfileMenuView ---------------------------------------------------------

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(views::Button* anchor_button, Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
  set_close_on_deactivate(close_on_deactivate_for_testing_);
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::BuildMenu() {
  Profile* profile = browser()->profile();
  if (profile->IsRegularProfile()) {
    BuildIdentity();
    BuildSyncInfo();
    BuildAutofillButtons();
  } else if (profile->IsGuestSession()) {
    BuildGuestIdentity();
  } else {
    NOTREACHED();
  }

  BuildFeatureButtons();
  BuildProfileManagementHeading();
  BuildSelectableProfiles();
  BuildProfileManagementFeatureButtons();
}

base::string16 ProfileMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  RecordClick(ActionableItem::kManageGoogleAccountButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_ManageGoogleAccountClicked"));

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasUnconsentedPrimaryAccount());

  NavigateToGoogleAccountPage(
      profile, identity_manager->GetUnconsentedPrimaryAccountInfo().email);
}

void ProfileMenuView::OnPasswordsButtonClicked() {
  RecordClick(ActionableItem::kPasswordsButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_PasswordsClicked"));
  NavigateToManagePasswordsPage(
      browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
}

void ProfileMenuView::OnCreditCardsButtonClicked() {
  RecordClick(ActionableItem::kCreditCardsButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_PaymentsClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
}

void ProfileMenuView::OnAddressesButtonClicked() {
  RecordClick(ActionableItem::kAddressesButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_AddressesClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
}

void ProfileMenuView::OnGuestProfileButtonClicked() {
  RecordClick(ActionableItem::kGuestProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_GuestClicked"));
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  RecordClick(ActionableItem::kManageProfilesButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_ManageClicked"));
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void ProfileMenuView::OnExitProfileButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_CloseAllClicked"));
  profiles::CloseProfileWindows(browser()->profile());
}

void ProfileMenuView::OnSyncSettingsButtonClicked() {
  RecordClick(ActionableItem::kSyncSettingsButton);
  chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
}

void ProfileMenuView::OnSyncErrorButtonClicked(
    sync_ui_util::AvatarSyncErrorType error) {
  RecordClick(ActionableItem::kSyncErrorButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_SignInAgainClicked"));
  switch (error) {
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
      chrome::ShowSettingsSubPage(browser(), chrome::kSignOutSubPage);
      break;
    case sync_ui_util::UNRECOVERABLE_ERROR:
      if (ProfileSyncServiceFactory::GetForProfile(browser()->profile())) {
        syncer::RecordSyncEvent(syncer::STOP_FROM_OPTIONS);
      }

      // GetPrimaryAccountMutator() might return nullptr on some platforms.
      if (auto* account_mutator =
              IdentityManagerFactory::GetForProfile(browser()->profile())
                  ->GetPrimaryAccountMutator()) {
        account_mutator->ClearPrimaryAccount(
            signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
            signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
        Hide();
        browser()->signin_view_controller()->ShowSignin(
            profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(),
            signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      }
      break;
    case sync_ui_util::AUTH_ERROR:
      Hide();
      browser()->signin_view_controller()->ShowSignin(
          profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH, browser(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
      break;
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      chrome::OpenUpdateChromeDialog(browser());
      break;
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR:
      sync_ui_util::OpenTabForSyncKeyRetrieval(browser());
      break;
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
    case sync_ui_util::NO_SYNC_ERROR:
      NOTREACHED();
      break;
  }
}

void ProfileMenuView::OnSigninButtonClicked() {
  RecordClick(ActionableItem::kSigninButton);
  Hide();
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

void ProfileMenuView::OnSigninAccountButtonClicked(AccountInfo account) {
  RecordClick(ActionableItem::kSigninAccountButton);
  Hide();
  signin_ui_util::EnableSyncFromPromo(
      browser(), account,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      true /* is_default_promo_account */);
}

void ProfileMenuView::OnSignoutButtonClicked() {
  RecordClick(ActionableItem::kSignoutButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("Signin_Signout_FromUserMenu"));
  Hide();
  // Sign out from all accounts.
  IdentityManagerFactory::GetForProfile(browser()->profile())
      ->GetAccountsMutator()
      ->RemoveAllAccounts(signin_metrics::SourceForRefreshTokenOperation::
                              kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  RecordClick(ActionableItem::kOtherProfileButton);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(base::UserMetricsAction("ProfileChooser_ProfileClicked"));
  Hide();
  profiles::SwitchToProfile(profile_path, /*always_create=*/false,
                            ProfileManager::CreateCallback(),
                            ProfileMetrics::SWITCH_PROFILE_ICON);
}

void ProfileMenuView::OnCookiesClearedOnExitLinkClicked() {
  RecordClick(ActionableItem::kCookiesClearedOnExitLink);
  // TODO(crbug.com/995757): Remove user action.
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_CookieSettingsClicked"));
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
}

void ProfileMenuView::OnAddNewProfileButtonClicked() {
  RecordClick(ActionableItem::kAddNewProfileButton);
  UserManager::Show(/*profile_path_to_focus=*/base::FilePath(),
                    profiles::USER_MANAGER_OPEN_CREATE_USER_PAGE);
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  RecordClick(ActionableItem::kEditProfileButton);
  chrome::ShowSettingsSubPage(browser(), chrome::kManageProfileSubPage);
}

void ProfileMenuView::BuildIdentity() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetUnconsentedPrimaryAccountInfo();
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          account);
  ProfileAttributesEntry* profile_attributes =
      GetProfileAttributesEntry(profile);
  size_t num_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();

  if (num_of_profiles > 1 || !profile_attributes->IsUsingDefaultName()) {
    SetHeading(profile_attributes->GetLocalProfileName(),
               l10n_util::GetStringUTF16(IDS_SETTINGS_EDIT_PERSON),
               base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                                   base::Unretained(this)));
  }

  if (account_info.has_value()) {
    SetIdentityInfo(
        account_info.value().account_image.AsImageSkia(), GetSyncIcon(),
        base::UTF8ToUTF16(account_info.value().full_name),
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.value().email));
  } else {
    SetIdentityInfo(
        profile_attributes->GetAvatarIcon().AsImageSkia(), GetSyncIcon(),
        /*title=*/base::string16(),
        l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  SetIdentityInfo(profiles::GetGuestAvatar(), GetSyncIcon(),
                  l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME));
}

gfx::ImageSkia ProfileMenuView::GetSyncIcon() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  if (!profile->IsRegularProfile())
    return gfx::ImageSkia();

  if (!identity_manager->HasPrimaryAccount())
    return ColoredImageForMenu(kSyncPausedCircleIcon, gfx::kGoogleGrey500);

  const gfx::VectorIcon* icon = nullptr;
  ui::NativeTheme::ColorId color_id;
  int unused;
  switch (
      sync_ui_util::GetMessagesForAvatarSyncError(profile, &unused, &unused)) {
    case sync_ui_util::NO_SYNC_ERROR:
      icon = &kSyncCircleIcon;
      color_id = ui::NativeTheme::kColorId_AlertSeverityLow;
      break;
    case sync_ui_util::AUTH_ERROR:
      icon = &kSyncPausedCircleIcon;
      color_id = ui::NativeTheme::kColorId_ProminentButtonColor;
      break;
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
    case sync_ui_util::UNRECOVERABLE_ERROR:
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
    case sync_ui_util::PASSPHRASE_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_EVERYTHING_ERROR:
    case sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR:
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      icon = &kSyncPausedCircleIcon;
      color_id = ui::NativeTheme::kColorId_AlertSeverityHigh;
      break;
  }
  return ColoredImageForMenu(*icon, native_theme->GetSystemColor(color_id));
}

void ProfileMenuView::BuildAutofillButtons() {
  AddShortcutFeatureButton(
      ImageForMenu(kKeyIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      ImageForMenu(kCreditCardIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      ImageForMenu(vector_icons::kLocationOnIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
  Profile* profile = browser()->profile();
  // Only show the sync info if signin and sync are allowed.
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      !ProfileSyncServiceFactory::IsSyncAllowed(profile)) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager->HasPrimaryAccount()) {
    // Show sync state.
    int description_string_id, button_string_id;
    sync_ui_util::AvatarSyncErrorType error =
        sync_ui_util::GetMessagesForAvatarSyncError(
            browser()->profile(), &description_string_id, &button_string_id);

    if (error == sync_ui_util::NO_SYNC_ERROR) {
      SetSyncInfo(
          GetSyncIcon(),
          /*description=*/base::string16(),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON),
          base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                              base::Unretained(this)));
    } else {
      const bool sync_paused = (error == sync_ui_util::AUTH_ERROR);
      const bool passwords_only_error =
          (error ==
           sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR);

      // Overwrite error description with short version for the menu.
      description_string_id =
          sync_paused
              ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
              : passwords_only_error ? IDS_SYNC_ERROR_PASSWORDS_USER_MENU_TITLE
                                     : IDS_SYNC_ERROR_USER_MENU_TITLE;

      SetSyncInfo(
          GetSyncIcon(), l10n_util::GetStringUTF16(description_string_id),
          l10n_util::GetStringUTF16(button_string_id),
          base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                              base::Unretained(this), error));
      SetSyncInfoBackgroundColor(GetSyncErrorBackgroundColor(sync_paused));
    }
    return;
  }

  // Show sync promos.
  CoreAccountInfo unconsented_account =
      identity_manager->GetUnconsentedPrimaryAccountInfo();
  base::Optional<AccountInfo> account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          unconsented_account);

  if (account_info.has_value()) {
    SetSyncInfo(
        GetSyncIcon(),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE),
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnSigninAccountButtonClicked,
                            base::Unretained(this), account_info.value()));
  } else {
    SetSyncInfo(/*icon=*/gfx::ImageSkia(),
                l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO),
                l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON),
                base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                                    base::Unretained(this)));
  }

  SetSyncInfoBackgroundColor(
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor));
}

void ProfileMenuView::BuildFeatureButtons() {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool is_guest = profile->IsGuestSession();
  const bool has_unconsented_account =
      !is_guest && identity_manager->HasUnconsentedPrimaryAccount();
  const bool has_primary_account =
      !is_guest && identity_manager->HasPrimaryAccount();

  if (has_unconsented_account && !IsSyncPaused(profile)) {
    AddFeatureButton(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        // The Google G icon needs to be shrunk, so it won't look too big
        // compared to the other icons.
        ImageForMenu(kGoogleGLogoIcon, /*icon_to_image_ratio=*/0.75),
#else
        gfx::ImageSkia(),
#endif
        l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
        base::BindRepeating(
            &ProfileMenuView::OnManageGoogleAccountButtonClicked,
            base::Unretained(this)));
  }

  int window_count = CountBrowsersFor(profile);
  if (window_count > 1) {
    AddFeatureButton(
        ImageForMenu(vector_icons::kCloseIcon),
        l10n_util::GetPluralStringFUTF16(IDS_PROFILES_CLOSE_X_WINDOWS_BUTTON,
                                         window_count),
        base::BindRepeating(&ProfileMenuView::OnExitProfileButtonClicked,
                            base::Unretained(this)));
  }

  // The sign-out button is always at the bottom.
  if (has_unconsented_account && !has_primary_account) {
    AddFeatureButton(
        ImageForMenu(kSignOutIcon),
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
        base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementHeading() {
  SetProfileManagementHeading(
      l10n_util::GetStringUTF16(IDS_PROFILES_OTHER_PROFILES_TITLE));
}

void ProfileMenuView::BuildSelectableProfiles() {
  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByName();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath())
      continue;

    AddSelectableProfile(
        profile_entry->GetAvatarIcon().AsImageSkia(), profile_entry->GetName(),
        /*is_guest=*/false,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }
  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        profile_entries.size() > 1);

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (!browser()->profile()->IsGuestSession() &&
      service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
    AddSelectableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME),
        /*is_guest=*/true,
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  AddProfileManagementShortcutFeatureButton(
      ImageForMenu(vector_icons::kSettingsIcon, kShortcutIconToImageRatio),
      l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_USERS_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                          base::Unretained(this)));

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  if (service->GetBoolean(prefs::kBrowserAddPersonEnabled)) {
    AddProfileManagementFeatureButton(
        ImageForMenu(kAddIcon, /*icon_to_image_ratio=*/0.75),
        l10n_util::GetStringUTF16(IDS_ADD),
        base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                            base::Unretained(this)));
  }
}
