// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/views/controls/styled_label.h"

namespace views {
class Button;
}

class Browser;
class DiceSigninButtonView;
class HoverButton;


// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public ProfileMenuViewBase,
                           public AvatarMenuObserver,
                           public signin::IdentityManager::Observer {
 public:
  ProfileChooserView(views::Button* anchor_button,
                     Browser* browser,
                     profiles::BubbleViewMode view_mode,
                     signin::GAIAServiceType service_type,
                     signin_metrics::AccessPoint access_point);
  ~ProfileChooserView() override;

 private:
  friend class ProfileChooserViewExtensionsTest;

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;

  // ProfileMenuViewBase:
  void FocusButtonOnKeyboardOpen() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnWidgetClosing(views::Widget* widget) override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetAccessibleWindowTitle() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // signin::IdentityManager::Observer overrides.
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  void Reset();

  // Shows the bubble with the |view_to_display|.
  void ShowView(profiles::BubbleViewMode view_to_display,
                AvatarMenu* avatar_menu);
  // Shows the bubble view or opens a tab based on given |mode|.
  void ShowViewOrOpenTab(profiles::BubbleViewMode mode);

  // Adds the profile chooser view.
  void AddProfileChooserView(AvatarMenu* avatar_menu);

  // Adds the main profile card for the profile |avatar_item|. |is_guest| is
  // used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  void AddCurrentProfileView(const AvatarMenu::Item& avatar_item,
                             bool is_guest);
  void AddGuestProfileView();
  void AddOptionsView(bool display_lock, AvatarMenu* avatar_menu);
  void AddSupervisedUserDisclaimerView();
  void AddAutofillHomeView();
#if defined(GOOGLE_CHROME_BUILD)
  void AddManageGoogleAccountButton();
#endif

  // Adds the DICE UI view to sign in and turn on sync. It includes an
  // illustration, a promo and a button.
  void AddDiceSigninView();

  // Adds a header for signin and sync error surfacing for the user menu.
  // Returns true if header is created.
  bool AddSyncErrorViewIfNeeded(const AvatarMenu::Item& avatar_item);

  // Adds a view showing a sync error and an error button, when dice is not
  // enabled.
  void AddPreDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                               sync_ui_util::AvatarSyncErrorType error,
                               int button_string_id,
                               int content_string_id);

  // Adds a view showing the profile associated with |avatar_item| and an error
  // button below, when dice is enabled.
  void AddDiceSyncErrorView(const AvatarMenu::Item& avatar_item,
                            sync_ui_util::AvatarSyncErrorType error,
                            int button_string_id);

  // Add a view showing that the reason for the sync paused is in the cookie
  // settings setup. On click, will direct to the cookie settings page.
  void AddSyncPausedReasonCookiesClearedOnExit();

  // Adds a promo for signin, if dice is not enabled.
  void AddPreDiceSigninPromo();

  // Adds a promo for signin, if dice is enabled.
  void AddDiceSigninPromo();

  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  // Callbacks for DiceAccountsMenu.
  void EnableSync(const base::Optional<AccountInfo>& account);
  void SignOutAllWebAccounts();

  // Methods to keep track of the number of times the Dice sign-in promo has
  // been shown.
  int GetDiceSigninPromoShowCount() const;
  void IncrementDiceSigninPromoShowCount();

  std::unique_ptr<AvatarMenu> avatar_menu_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Button in the signin/sync error header on top of the desktop user menu.
  views::Button* sync_error_button_;

  // Links and buttons displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::Button* manage_accounts_button_;
  views::Button* signin_current_profile_button_;
  HoverButton* sync_to_another_account_button_;
  views::Button* signin_with_gaia_account_button_;

  // For material design user menu, the active profile card owns the profile
  // name and photo.
  views::Button* current_profile_card_;

  // Action buttons.
  views::Button* first_profile_button_;
  views::Button* guest_profile_button_;
  views::Button* users_button_;
  views::Button* lock_button_;
  views::Button* close_all_windows_button_;
  views::Button* passwords_button_;
  views::Button* credit_cards_button_;
  views::Button* addresses_button_;
  views::Button* signout_button_;
  views::Button* manage_google_account_button_;

  views::StyledLabel* cookies_cleared_on_exit_label_;

  // View for the signin/turn-on-sync button in the dice promo.
  DiceSigninButtonView* dice_signin_button_view_;

  // Active view mode.
  profiles::BubbleViewMode view_mode_;

  // The GAIA service type provided in the response header.
  signin::GAIAServiceType gaia_service_type_;

  // The current access point of sign in.
  const signin_metrics::AccessPoint access_point_;

  // Dice accounts used in the sync promo.
  std::vector<AccountInfo> dice_accounts_;

  // Accounts submenu that is shown when |sync_to_another_account_button_| is
  // pressed.
  std::unique_ptr<DiceAccountsMenu> dice_accounts_menu_;

  const bool dice_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
