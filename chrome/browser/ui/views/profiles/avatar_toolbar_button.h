// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/avatar_button_error_controller.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/events/event.h"

class Browser;

class AvatarToolbarButton : public ToolbarButton,
                            public AvatarButtonErrorControllerDelegate,
                            public BrowserListObserver,
                            public ProfileAttributesStorage::Observer,
                            public signin::IdentityManager::Observer,
                            public ui::MaterialDesignControllerObserver {
 public:
  explicit AvatarToolbarButton(Browser* browser);
  ~AvatarToolbarButton() override;

  void UpdateIcon();
  void UpdateText();
  void SetSuppressAvatarButtonState(bool suppress_avatar_button_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);
  enum class SyncState { kNormal, kPaused, kError };

  // ToolbarButton:
  void NotifyClick(const ui::Event& event) override;
  void OnThemeChanged() override;
  void AddedToWidget() override;

  // AvatarButtonErrorControllerDelegate:
  void OnAvatarErrorChanged() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  bool IsIncognito() const;
  bool IsIncognitoCounterActive() const;
  bool ShouldShowGenericIcon() const;
  base::string16 GetAvatarTooltipText() const;
  gfx::ImageSkia GetAvatarIcon() const;
  gfx::Image GetIconImageFromProfile() const;
  SyncState GetSyncState() const;

  void SetInsets();

  Browser* const browser_;
  Profile* const profile_;

  // Indicates if the avatar icon should show text and update highlight color
  // when sync state is not normal.
  bool suppress_avatar_button_state_ = false;

#if !defined(OS_CHROMEOS)
  AvatarButtonErrorController error_controller_;
#endif  // !defined(OS_CHROMEOS)
  ScopedObserver<BrowserList, BrowserListObserver> browser_list_observer_;
  ScopedObserver<ProfileAttributesStorage, AvatarToolbarButton>
      profile_observer_;
  ScopedObserver<signin::IdentityManager, AvatarToolbarButton>
      identity_manager_observer_;
  ScopedObserver<ui::MaterialDesignController, AvatarToolbarButton>
      md_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AvatarToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
