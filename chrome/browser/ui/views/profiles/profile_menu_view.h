// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "ui/views/controls/styled_label.h"

namespace views {
class Button;
}

class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
// TODO(crbug.com/993752): Remove AvatarMenuObserver after ProfileMenuRevamp.
class ProfileMenuView : public ProfileMenuViewBase, public AvatarMenuObserver {
 public:
  ProfileMenuView(views::Button* anchor_button,
                     Browser* browser,
                     signin_metrics::AccessPoint access_point);
  ~ProfileMenuView() override;

  // ProfileMenuViewBase:
  void BuildMenu() override;

 private:
  friend class ProfileMenuViewExtensionsTest;

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // ProfileMenuViewBase:
  void FocusButtonOnKeyboardOpen() override;

  // views::BubbleDialogDelegateView:
  void OnWidgetClosing(views::Widget* widget) override;
  base::string16 GetAccessibleWindowTitle() const override;

  // Button/link actions.
  void OnManageGoogleAccountButtonClicked();
  void OnPasswordsButtonClicked();
  void OnCreditCardsButtonClicked();
  void OnAddressesButtonClicked();
  void OnGuestProfileButtonClicked();
  void OnManageProfilesButtonClicked();
  void OnExitProfileButtonClicked();
  void OnSyncSettingsButtonClicked();
  void OnSyncErrorButtonClicked(sync_ui_util::AvatarSyncErrorType error);
  void OnSigninButtonClicked();
  void OnSigninAccountButtonClicked(AccountInfo account);
  void OnSignoutButtonClicked();
  void OnOtherProfileSelected(const base::FilePath& profile_path);
  void OnCookiesClearedOnExitLinkClicked();
  void OnAddNewProfileButtonClicked();
  void OnEditProfileButtonClicked();

  // TODO(crbug.com/1021587): Remove methods after ProfileMenuRevamp.
  void OnLockButtonClicked();
  void OnCurrentProfileCardClicked();

  // TODO(crbug.com/993752): Remove after ProfileMenuRevamp.
  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  // Helper methods for building the menu.
  void BuildIdentity();
  void BuildGuestIdentity();
  gfx::ImageSkia GetSyncIcon();
  void BuildAutofillButtons();
  void BuildSyncInfo();
  void BuildFeatureButtons();
  void BuildProfileManagementHeading();
  void BuildSelectableProfiles();
  void BuildProfileManagementFeatureButtons();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds the profile chooser view.
  void AddProfileMenuView(AvatarMenu* avatar_menu);

  // TODO(crbug.com/1021587): Remove methods below after ProfileMenuRevamp.
  // Adds the main profile card for the profile |avatar_item|. |is_guest| is
  // used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  void AddCurrentProfileView(const AvatarMenu::Item& avatar_item,
                             bool is_guest);
  void AddGuestProfileView();
  void AddOptionsView(bool display_lock, AvatarMenu* avatar_menu);
  void AddSupervisedUserDisclaimerView();
  void AddAutofillHomeView();
  void AddManageGoogleAccountButton();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds the DICE UI view to sign in and turn on sync. It includes an
  // illustration, a promo and a button.
  void AddDiceSigninView();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds a header for signin and sync error surfacing for the user menu.
  // Returns true if header is created.
  bool AddSyncErrorViewIfNeeded(const AvatarMenu::Item& avatar_item);

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds a view showing a sync error and an error button, when dice is not
  // enabled.
  void AddPreDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                               sync_ui_util::AvatarSyncErrorType error,
                               int button_string_id,
                               int content_string_id);

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds a view showing the profile associated with |avatar_item| and an error
  // button below, when dice is enabled.
  void AddDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                            sync_ui_util::AvatarSyncErrorType error,
                            int button_string_id);

  // TODO(crbug.com/1021587): Add to new profile menu.
  // Add a view showing that the reason for the sync paused is in the cookie
  // settings setup. On click, will direct to the cookie settings page.
  void AddSyncPausedReasonCookiesClearedOnExit();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds a promo for signin, if dice is not enabled.
  void AddPreDiceSigninPromo();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Adds a promo for signin, if dice is enabled.
  void AddDiceSigninPromo();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Methods to keep track of the number of times the Dice sign-in promo has
  // been shown.
  int GetDiceSigninPromoShowCount() const;
  void IncrementDiceSigninPromoShowCount();

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  std::unique_ptr<AvatarMenu> avatar_menu_;

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Button pointers used in tests.
  views::Button* first_profile_button_ = nullptr;
  views::Button* lock_button_ = nullptr;

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // The current access point of sign in.
  const signin_metrics::AccessPoint access_point_;

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  // Dice accounts used in the sync promo.
  std::vector<AccountInfo> dice_accounts_;

  // TODO(crbug.com/1021587): Remove after ProfileMenuRevamp.
  const bool dice_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
