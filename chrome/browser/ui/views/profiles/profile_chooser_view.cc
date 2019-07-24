// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

// Helpers --------------------------------------------------------------------


// Number of times the Dice sign-in promo illustration should be shown.
constexpr int kDiceSigninPromoIllustrationShowCountMax = 10;

bool IsProfileChooser(profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
}

BadgedProfilePhoto::BadgeType GetProfileBadgeType(Profile* profile) {
  if (profile->IsSupervised()) {
    return profile->IsChild() ? BadgedProfilePhoto::BADGE_TYPE_CHILD
                              : BadgedProfilePhoto::BADGE_TYPE_SUPERVISOR;
  }
  // |Profile::IsSyncAllowed| is needed to check whether sync is allowed by GPO
  // policy.
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      profile->IsSyncAllowed() &&
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount()) {
    return BadgedProfilePhoto::BADGE_TYPE_SYNC_COMPLETE;
  }
  return BadgedProfilePhoto::BADGE_TYPE_NONE;
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

bool AreSigninCookiesClearedOnExit(Profile* profile) {
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
  return client->AreSigninCookiesDeletedOnExit();
}

#if defined(GOOGLE_CHROME_BUILD)
// Returns the Google G icon in grey and with a padding of 2. The padding is
// needed to make the icon look smaller, otherwise it looks too big compared to
// the other icons. See crbug.com/951751 for more information.
gfx::ImageSkia GetGoogleIconForUserMenu(int icon_size) {
  constexpr int kIconPadding = 2;
  SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);
  // |CreateVectorIcon()| doesn't override colors specified in the .icon file,
  // therefore the image has to be colored manually with |CreateColorMask()|.
  gfx::ImageSkia google_icon = gfx::CreateVectorIcon(
      kGoogleGLogoIcon, icon_size - 2 * kIconPadding, gfx::kPlaceholderColor);
  gfx::ImageSkia grey_google_icon =
      gfx::ImageSkiaOperations::CreateColorMask(google_icon, icon_color);

  return gfx::CanvasImageSource::CreatePadded(grey_google_icon,
                                              gfx::Insets(kIconPadding));
}
#endif

}  // namespace

// ProfileChooserView ---------------------------------------------------------

// static
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

ProfileChooserView::ProfileChooserView(views::Button* anchor_button,
                                       Browser* browser,
                                       profiles::BubbleViewMode view_mode,
                                       signin::GAIAServiceType service_type,
                                       signin_metrics::AccessPoint access_point)
    : ProfileMenuViewBase(anchor_button, browser),
      view_mode_(view_mode),
      gaia_service_type_(service_type),
      access_point_(access_point),
      dice_enabled_(AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser->profile())) {
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
  base::RecordAction(base::UserMetricsAction("ProfileChooser_Show"));
}

ProfileChooserView::~ProfileChooserView() = default;

void ProfileChooserView::Reset() {
  ProfileMenuViewBase::Reset();
  open_other_profile_indexes_map_.clear();
  sync_error_button_ = nullptr;
  signin_current_profile_button_ = nullptr;
  signin_with_gaia_account_button_ = nullptr;
  current_profile_card_ = nullptr;
  first_profile_button_ = nullptr;
  guest_profile_button_ = nullptr;
  users_button_ = nullptr;
  lock_button_ = nullptr;
  close_all_windows_button_ = nullptr;
  dice_signin_button_view_ = nullptr;
  passwords_button_ = nullptr;
  credit_cards_button_ = nullptr;
  addresses_button_ = nullptr;
  signout_button_ = nullptr;
  manage_google_account_button_ = nullptr;
  cookies_cleared_on_exit_label_ = nullptr;
}

void ProfileChooserView::Init() {
  Reset();
  set_close_on_deactivate(close_on_deactivate_for_testing_);

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      this, browser()));
  avatar_menu_->RebuildMenu();

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager)
    identity_manager->AddObserver(this);

  if (dice_enabled_) {
    // Fetch DICE accounts. Note: This always includes the primary account if it
    // is set.
    dice_accounts_ =
        signin_ui_util::GetAccountsForDicePromos(browser()->profile());
  }

  ShowViewOrOpenTab(view_mode_);
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  if (IsProfileChooser(view_mode_)) {
    // Refresh the view with the new menu. We can't just update the local copy
    // as this may have been triggered by a sign out action, in which case
    // the view is being destroyed.
    ShowView(view_mode_, avatar_menu);
  }
}

void ProfileChooserView::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    ShowViewOrOpenTab(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  }
}

void ProfileChooserView::ShowView(profiles::BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  if (browser()->profile()->IsSupervised() &&
      view_to_display == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT) {
    LOG(WARNING) << "Supervised user attempted to add account";
    return;
  }

  view_mode_ = view_to_display;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      // The modal sign-in view is shown in for bubble view modes.
      // See |SigninViewController::ShouldShowSigninForMode|.
      NOTREACHED();
      break;
    case profiles::BUBBLE_VIEW_MODE_INCOGNITO:
      // Covered in IncognitoView.
      NOTREACHED();
      break;
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
      AddProfileChooserView(avatar_menu);
      break;
  }
  RepopulateViewFromMenuItems();
}

void ProfileChooserView::ShowViewOrOpenTab(profiles::BubbleViewMode mode) {
  if (SigninViewController::ShouldShowSigninForMode(mode)) {
    // Hides the user menu if it is currently shown. The user menu automatically
    // closes when it loses focus; however, on Windows, the signin modals do not
    // take away focus, thus we need to manually close the bubble.
    Hide();
    browser()->signin_view_controller()->ShowSignin(mode, browser(),
                                                    access_point_);
  } else {
    ShowView(mode, avatar_menu_.get());
  }
}

void ProfileChooserView::FocusButtonOnKeyboardOpen() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileChooserView::OnWidgetClosing(views::Widget* /*widget*/) {
  // Unsubscribe from everything early so that the updates do not reach the
  // bubble and change its state.
  avatar_menu_.reset();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  if (identity_manager)
    identity_manager->RemoveObserver(this);
}

views::View* ProfileChooserView::GetInitiallyFocusedView() {
  return ShouldProvideInitiallyFocusedView() ? signin_current_profile_button_
                                             : nullptr;
}

base::string16 ProfileChooserView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == manage_google_account_button_) {
    DCHECK(!dice_accounts_.empty());
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_ManageGoogleAccountClicked"));
    NavigateToGoogleAccountPage(browser()->profile(), dice_accounts_[0].email);
  } else if (sender == passwords_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PasswordsClicked"));
    NavigateToManagePasswordsPage(
        browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
  } else if (sender == credit_cards_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PaymentsClicked"));
    chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
  } else if (sender == addresses_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_AddressesClicked"));
    chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
  } else if (sender == guest_profile_button_) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
    profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
    base::RecordAction(base::UserMetricsAction("ProfileChooser_GuestClicked"));
  } else if (sender == users_button_) {
    // If this is a guest session, close all the guest browser windows.
    if (browser()->profile()->IsGuestSession()) {
      profiles::CloseGuestProfileWindows();
    } else {
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ManageClicked"));
      UserManager::Show(base::FilePath(),
                        profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    }
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER);
  } else if (sender == lock_button_) {
    profiles::LockProfile(browser()->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
  } else if (sender == close_all_windows_button_) {
    profiles::CloseProfileWindows(browser()->profile());
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_CloseAllClicked"));
  } else if (sender == sync_error_button_) {
    sync_ui_util::AvatarSyncErrorType error =
        static_cast<sync_ui_util::AvatarSyncErrorType>(sender->GetID());
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
          ShowViewOrOpenTab(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
        }
        break;
      case sync_ui_util::AUTH_ERROR:
        ShowViewOrOpenTab(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
        break;
      case sync_ui_util::UPGRADE_CLIENT_ERROR:
        chrome::OpenUpdateChromeDialog(browser());
        break;
      case sync_ui_util::PASSPHRASE_ERROR:
      case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
        chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
        break;
      case sync_ui_util::NO_SYNC_ERROR:
        NOTREACHED();
        break;
    }
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_SignInAgainClicked"));
  } else if (sender == current_profile_card_) {
    if (dice_enabled_ &&
        IdentityManagerFactory::GetForProfile(browser()->profile())
            ->HasPrimaryAccount()) {
      chrome::ShowSettingsSubPage(browser(), chrome::kPeopleSubPage);
    } else {
      // Open settings to edit profile name and image. The profile doesn't need
      // to be authenticated to open this.
      avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
    }
  } else if (sender == signin_current_profile_button_) {
    ShowViewOrOpenTab(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
  } else if (sender == signin_with_gaia_account_button_) {
    DCHECK(dice_signin_button_view_->account());
    Hide();
    signin_ui_util::EnableSyncFromPromo(
        browser(), dice_signin_button_view_->account().value(), access_point_,
        true /* is_default_promo_account */);
  } else if (sender == signout_button_) {
    SignOutAllWebAccounts();
    base::RecordAction(base::UserMetricsAction("Signin_Signout_FromUserMenu"));
  } else {
    // Either one of the "other profiles", or one of the profile accounts
    // buttons was pressed.
    ButtonIndexes::const_iterator profile_match =
        open_other_profile_indexes_map_.find(sender);
    if (profile_match != open_other_profile_indexes_map_.end()) {
      avatar_menu_->SwitchToProfile(
          profile_match->second, ui::DispositionFromEventFlags(event.flags()) ==
                                     WindowOpenDisposition::NEW_WINDOW,
          ProfileMetrics::SWITCH_PROFILE_ICON);
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ProfileClicked"));
      Hide();
    } else {
      NOTREACHED();
    }
  }
}

void ProfileChooserView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
  DCHECK_EQ(cookies_cleared_on_exit_label_, label);
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_CookieSettingsClicked"));
}

void ProfileChooserView::AddProfileChooserView(AvatarMenu* avatar_menu) {
  // Separate items into active and alternatives.
  const AvatarMenu::Item* active_item = nullptr;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active) {
      active_item = &avatar_menu->GetItemAt(i);
      break;
    }
  }

  bool sync_error =
      active_item ? AddSyncErrorViewIfNeeded(*active_item) : false;

  if (!sync_error || !dice_enabled_) {
    // Guest windows don't have an active profile.
    if (active_item)
      AddCurrentProfileView(*active_item, /* is_guest = */ false);
    else
      AddGuestProfileView();
  }

#if defined(GOOGLE_CHROME_BUILD)
  if (dice_enabled_ && !dice_accounts_.empty() &&
      !SigninErrorControllerFactory::GetForProfile(browser()->profile())
           ->HasError()) {
    AddManageGoogleAccountButton();
  }
#endif

  if (browser()->profile()->IsSupervised())
    AddSupervisedUserDisclaimerView();

  if (active_item)
    AddAutofillHomeView();

  const bool display_lock = active_item && active_item->signed_in &&
                            profiles::IsLockAvailable(browser()->profile());
  AddOptionsView(display_lock, avatar_menu);
}

bool ProfileChooserView::AddSyncErrorViewIfNeeded(
    const AvatarMenu::Item& avatar_item) {
  int content_string_id, button_string_id;
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser()->profile(), &content_string_id, &button_string_id);
  if (error == sync_ui_util::NO_SYNC_ERROR)
    return false;

  if (dice_enabled_) {
    AddDiceSyncErrorView(avatar_item, error, button_string_id);
  } else {
    AddPreDiceSyncErrorView(avatar_item, error, button_string_id,
                            content_string_id);
  }

  return true;
}

void ProfileChooserView::AddPreDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id,
    int content_string_id) {
  AddMenuGroup();
  auto sync_problem_icon = std::make_unique<views::ImageView>();
  sync_problem_icon->SetImage(gfx::CreateVectorIcon(
      kSyncProblemIcon, BadgedProfilePhoto::kImageSize, gfx::kGoogleRed700));
  views::Button* button = CreateAndAddTitleCard(
      std::move(sync_problem_icon),
      l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE),
      l10n_util::GetStringUTF16(content_string_id), false);
  static_cast<HoverButton*>(button)->SetStyle(HoverButton::STYLE_ERROR);

  // Adds an action button if an action exists.
  if (button_string_id) {
    sync_error_button_ = CreateAndAddBlueButton(
        l10n_util::GetStringUTF16(button_string_id), true /* md_style */);
    // Track the error type so that the correct action can be taken in
    // ButtonPressed().
    sync_error_button_->SetID(error);
  }
}

void ProfileChooserView::AddDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id) {
  // Creates a view containing an error hover button displaying the current
  // profile (only selectable when sync is paused or disabled) and when sync is
  // not disabled there is a blue button to resolve the error.
  const bool show_sync_paused_ui = error == sync_ui_util::AUTH_ERROR;
  const bool sync_disabled = !browser()->profile()->IsSyncAllowed();

  AddMenuGroup();

  if (show_sync_paused_ui &&
      base::FeatureList::IsEnabled(
          features::kShowSyncPausedReasonCookiesClearedOnExit) &&
      AreSigninCookiesClearedOnExit(browser()->profile())) {
    AddSyncPausedReasonCookiesClearedOnExit();
  }
  // Add profile card.
  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      show_sync_paused_ui
          ? BadgedProfilePhoto::BADGE_TYPE_SYNC_PAUSED
          : sync_disabled ? BadgedProfilePhoto::BADGE_TYPE_SYNC_DISABLED
                          : BadgedProfilePhoto::BADGE_TYPE_SYNC_ERROR,
      avatar_item.icon);
  current_profile_card_ = CreateAndAddTitleCard(
      std::move(current_profile_photo),
      l10n_util::GetStringUTF16(
          show_sync_paused_ui
              ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
              : sync_disabled ? IDS_PROFILES_DICE_SYNC_DISABLED_TITLE
                              : IDS_SYNC_ERROR_USER_MENU_TITLE),
      avatar_item.username);

  if (!show_sync_paused_ui && !sync_disabled) {
    static_cast<HoverButton*>(current_profile_card_)
        ->SetStyle(HoverButton::STYLE_ERROR);
    current_profile_card_->SetEnabled(false);
  }

  if (!sync_disabled) {
    sync_error_button_ = CreateAndAddBlueButton(
        l10n_util::GetStringUTF16(button_string_id), true /* md_style */);
    sync_error_button_->SetID(error);
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_SignInAgainDisplayed"));
  }
}

void ProfileChooserView::AddSyncPausedReasonCookiesClearedOnExit() {
  size_t offset = 0;
  std::unique_ptr<views::StyledLabel> sync_paused_reason =
      std::make_unique<views::StyledLabel>(base::string16(), this);

  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_SYNC_PAUSED_REASON_CLEAR_COOKIES_ON_EXIT_LINK_TEXT);

  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_SYNC_PAUSED_REASON_CLEAR_COOKIES_ON_EXIT, link_text, &offset);

  sync_paused_reason->SetText(message);
  // Mark the link text as link.
  sync_paused_reason->AddStyleRange(
      gfx::Range(offset, offset + link_text.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  // Mark the rest of the text as secondary text.
  views::StyledLabel::RangeStyleInfo message_style;
  message_style.text_style = STYLE_SECONDARY;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    sync_paused_reason->AddStyleRange(before_link_range, message_style);

  gfx::Range after_link_range(offset + link_text.length(), message.length());
  if (!after_link_range.is_empty())
    sync_paused_reason->AddStyleRange(after_link_range, message_style);

  cookies_cleared_on_exit_label_ = sync_paused_reason.get();
  AddViewItem(std::move(sync_paused_reason));
}

void ProfileChooserView::AddCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  Profile* profile = browser()->profile();
  const bool sync_disabled = !profile->IsSyncAllowed();
  if (!is_guest && sync_disabled) {
    AddDiceSyncErrorView(avatar_item, sync_ui_util::NO_SYNC_ERROR, 0);
    return;
  }

  if (!avatar_item.signed_in && dice_enabled_ &&
      SyncPromoUI::ShouldShowSyncPromo(profile)) {
    AddDiceSigninView();
    return;
  }

  AddMenuGroup();

  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      GetProfileBadgeType(profile), avatar_item.icon);
  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile->GetPath());

  // Show the profile name by itself if not signed in or account consistency is
  // disabled. Otherwise, show the email attached to the profile.
  bool show_email = !is_guest && avatar_item.signed_in;
  const base::string16 hover_button_title =
      dice_enabled_ && profile->IsSyncAllowed() && show_email
          ? l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_COMPLETE_TITLE)
          : profile_name;

  current_profile_card_ = CreateAndAddTitleCard(
      std::move(current_profile_photo), hover_button_title,
      show_email ? avatar_item.username : base::string16());
  // TODO(crbug.com/815047): Sometimes, |avatar_item.username| is empty when
  // |show_email| is true, which should never happen. This causes a crash when
  // setting the elision behavior, so until this bug is fixed, avoid the crash
  // by checking that the username is not empty.
  if (show_email && !avatar_item.username.empty())
    static_cast<HoverButton*>(current_profile_card_)
        ->SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);

  // The available links depend on the type of profile that is active.
  if (is_guest) {
    current_profile_card_->SetEnabled(false);
  } else if (avatar_item.signed_in) {
    current_profile_card_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME, profile_name,
        avatar_item.username));
  } else {
    bool is_signin_allowed =
        profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
    // For the dice promo equivalent, see AddDiceSigninPromo() call sites.
    if (!dice_enabled_ && is_signin_allowed)
      AddPreDiceSigninPromo();

    current_profile_card_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME, profile_name));
  }
}

void ProfileChooserView::AddPreDiceSigninPromo() {
  AddMenuGroup(false /* add_separator */);
  CreateAndAddLabel(l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));

  signin_current_profile_button_ = CreateAndAddBlueButton(
      l10n_util::GetStringFUTF16(
          IDS_SYNC_START_SYNC_BUTTON_LABEL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
      true /* md_style */);

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

void ProfileChooserView::AddDiceSigninPromo() {
  AddMenuGroup();

  // Show promo illustration + text when there is no promo account.
  if (GetDiceSigninPromoShowCount() <=
      kDiceSigninPromoIllustrationShowCountMax) {
    // Add the illustration.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    std::unique_ptr<NonAccessibleImageView> illustration =
        std::make_unique<NonAccessibleImageView>();
    illustration->SetImage(
        *rb.GetNativeImageNamed(IDR_PROFILES_DICE_TURN_ON_SYNC).ToImageSkia());
    AddViewItem(std::move(illustration));
  }
  // Add the promo text.
  CreateAndAddLabel(l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO));

  // Create a sign-in button without account information.
  std::unique_ptr<DiceSigninButtonView> signin_button =
      std::make_unique<DiceSigninButtonView>(this);
  dice_signin_button_view_ = CreateAndAddDiceSigninButton();
  signin_current_profile_button_ = dice_signin_button_view_->signin_button();
}

void ProfileChooserView::AddDiceSigninView() {
  IncrementDiceSigninPromoShowCount();
  // Create a view that holds an illustration, a promo text and a button to turn
  // on Sync. The promo illustration is only shown the first 10 times per
  // profile.
  const bool promo_account_available = !dice_accounts_.empty();

  // Log sign-in impressions user metrics.
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      promo_account_available);

  if (!promo_account_available) {
    // For the pre-dice promo equivalent, see AddPreDiceSigninPromo() call
    // sites.
    AddDiceSigninPromo();
    return;
  }

  AddMenuGroup();
  // Create a button to sign in the first account of |dice_accounts_|.
  AccountInfo dice_promo_default_account = dice_accounts_[0];
  gfx::Image account_icon = dice_promo_default_account.account_image;
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  dice_signin_button_view_ =
      CreateAndAddDiceSigninButton(&dice_promo_default_account, &account_icon);
  signin_with_gaia_account_button_ = dice_signin_button_view_->signin_button();

  // Add sign out button.
  signout_button_ = CreateAndAddBlueButton(
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT),
      false /* md_style */);
}

void ProfileChooserView::AddGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guest_avatar_item(0, base::FilePath(), guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  AddCurrentProfileView(guest_avatar_item, true);
}

void ProfileChooserView::AddOptionsView(bool display_lock,
                                        AvatarMenu* avatar_menu) {
  AddMenuGroup();

  const bool is_guest = browser()->profile()->IsGuestSession();
  // Add the user switching buttons.
  // Order them such that the active user profile comes first (for Dice).
  std::vector<size_t> ordered_item_indices;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active)
      ordered_item_indices.insert(ordered_item_indices.begin(), i);
    else
      ordered_item_indices.push_back(i);
  }
  for (size_t i : ordered_item_indices) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (!item.active) {
      gfx::Image image = profiles::GetSizedAvatarIcon(
          item.icon, true, GetDefaultIconSize(), GetDefaultIconSize(),
          profiles::SHAPE_CIRCLE);
      views::Button* button = CreateAndAddButton(
          *image.ToImageSkia(), profiles::GetProfileSwitcherTextForItem(item));
      open_other_profile_indexes_map_[button] = i;

      if (!first_profile_button_)
        first_profile_button_ = button;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        first_profile_button_);

  // Add the "Guest" button for browsing as guest
  if (!is_guest && !browser()->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      guest_profile_button_ = CreateAndAddButton(
          CreateVectorIcon(kUserMenuGuestIcon),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_GUEST_PROFILE_BUTTON));
    }
  }

  base::string16 text = l10n_util::GetStringUTF16(
      is_guest ? IDS_PROFILES_EXIT_GUEST : IDS_PROFILES_MANAGE_USERS_BUTTON);
  const gfx::VectorIcon& settings_icon =
      is_guest ? kCloseAllIcon : vector_icons::kSettingsIcon;
  users_button_ = CreateAndAddButton(CreateVectorIcon(settings_icon), text);

  if (display_lock) {
    lock_button_ = CreateAndAddButton(
        gfx::CreateVectorIcon(vector_icons::kLockIcon, GetDefaultIconSize(),
                              gfx::kChromeIconGrey),
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
  } else if (!is_guest) {
    AvatarMenu::Item active_avatar_item =
        avatar_menu->GetItemAt(ordered_item_indices[0]);
    close_all_windows_button_ = CreateAndAddButton(
        CreateVectorIcon(kCloseAllIcon),
        avatar_menu->GetNumberOfItems() >= 2
            ? l10n_util::GetStringFUTF16(IDS_PROFILES_EXIT_PROFILE_BUTTON,
                                         active_avatar_item.name)
            : l10n_util::GetStringUTF16(IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON));
  }
}

void ProfileChooserView::AddSupervisedUserDisclaimerView() {
  AddMenuGroup();
  auto* disclaimer = CreateAndAddLabel(
      avatar_menu_->GetSupervisedUserInformation(), CONTEXT_BODY_TEXT_SMALL);
  disclaimer->SetAllowCharacterBreak(true);
}

void ProfileChooserView::AddAutofillHomeView() {
  if (browser()->profile()->IsGuestSession())
    return;

  AddMenuGroup();

  // Passwords.
  passwords_button_ = CreateAndAddButton(
      CreateVectorIcon(kKeyIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK));

  // Credit cards.
  credit_cards_button_ = CreateAndAddButton(
      CreateVectorIcon(kCreditCardIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK));

  // Addresses.
  addresses_button_ = CreateAndAddButton(
      CreateVectorIcon(vector_icons::kLocationOnIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK));
}

#if defined(GOOGLE_CHROME_BUILD)
void ProfileChooserView::AddManageGoogleAccountButton() {
  AddMenuGroup(false);
  manage_google_account_button_ = CreateAndAddButton(
      GetGoogleIconForUserMenu(GetDefaultIconSize()),
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT));
}
#endif

void ProfileChooserView::PostActionPerformed(
    ProfileMetrics::ProfileDesktopMenu action_performed) {
  ProfileMetrics::LogProfileDesktopMenu(action_performed, gaia_service_type_);
  gaia_service_type_ = signin::GAIA_SERVICE_TYPE_NONE;
}

void ProfileChooserView::EnableSync(
    const base::Optional<AccountInfo>& account) {
  Hide();
  if (account)
    signin_ui_util::EnableSyncFromPromo(browser(), account.value(),
                                        access_point_,
                                        false /* is_default_promo_account */);
  else
    ShowViewOrOpenTab(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
}

void ProfileChooserView::SignOutAllWebAccounts() {
  Hide();
  IdentityManagerFactory::GetForProfile(browser()->profile())
      ->GetAccountsMutator()
      ->RemoveAllAccounts(signin_metrics::SourceForRefreshTokenOperation::
                              kUserMenu_SignOutAllAccounts);
}

int ProfileChooserView::GetDiceSigninPromoShowCount() const {
  return browser()->profile()->GetPrefs()->GetInteger(
      prefs::kDiceSigninUserMenuPromoCount);
}

void ProfileChooserView::IncrementDiceSigninPromoShowCount() {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, GetDiceSigninPromoShowCount() + 1);
}
