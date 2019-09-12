// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"

namespace {

Profile* CreateTestingProfile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  std::unique_ptr<Profile> profile =
      Profile::CreateProfile(path, nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  Profile* profile_ptr = profile.get();
  profile_manager->RegisterTestingProfile(std::move(profile), true, false);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile_ptr;
}

Profile* CreateTestingProfile(const std::string& profile_name) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(profile_name);
  return CreateTestingProfile(path);
}

// Turns a normal profile into one that's signed in.
void AddAccountToProfile(Profile* profile, const char* signed_in_email) {
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry_signed_in;
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                   &entry_signed_in));
  entry_signed_in->SetAuthInfo("12345", base::UTF8ToUTF16(signed_in_email));
  profile->GetPrefs()->SetString(prefs::kGoogleServicesHostedDomain,
                                 "google.com");
}

// Set up the profiles to enable Lock. Takes as parameter a profile that will be
// signed in, and also creates a supervised user (necessary for lock), then
// returns the supervised user profile.
Profile* SetupProfilesForLock(Profile* signed_in) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  constexpr char kEmail[] = "me@google.com";
  AddAccountToProfile(signed_in, kEmail);

  // Create the |supervised| profile, which is supervised by |signed_in|.
  ProfileAttributesEntry* entry_supervised;
  Profile* supervised = CreateTestingProfile("supervised");
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  EXPECT_TRUE(storage.GetProfileAttributesWithPath(supervised->GetPath(),
                                                   &entry_supervised));
  entry_supervised->SetSupervisedUserId(kEmail);
  supervised->GetPrefs()->SetString(prefs::kSupervisedUserId, kEmail);

  // |signed_in| should now be lockable.
  EXPECT_TRUE(profiles::IsLockAvailable(signed_in));
  return supervised;
}

}  // namespace

class ProfileMenuViewExtensionsTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ProfileMenuViewExtensionsTest() {}
  ~ProfileMenuViewExtensionsTest() override {}

  // SupportsTestUi:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    constexpr char kSignedIn[] = "SignedIn";
    constexpr char kMultiProfile[] = "MultiProfile";
    constexpr char kGuest[] = "Guest";
    constexpr char kDiceGuest[] = "DiceGuest";
    constexpr char kManageAccountLink[] = "ManageAccountLink";
    constexpr char kSupervisedOwner[] = "SupervisedOwner";
    constexpr char kSupervisedUser[] = "SupervisedUser";

    Browser* target_browser = browser();
    if (name == kSignedIn || name == kManageAccountLink) {
      constexpr char kEmail[] = "verylongemailfortesting@gmail.com";
      AddAccountToProfile(target_browser->profile(), kEmail);
    }
    if (name == kMultiProfile) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
      CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
    }
    if (name == kGuest || name == kDiceGuest) {
      profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());

      Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetGuestProfilePath());
      EXPECT_TRUE(guest);
      target_browser = chrome::FindAnyBrowser(guest, true);
    }

    Profile* supervised = nullptr;
    if (name == kSupervisedOwner || name == kSupervisedUser) {
      supervised = SetupProfilesForLock(target_browser->profile());
    }
    if (name == kSupervisedUser) {
      profiles::SwitchToProfile(supervised->GetPath(), false,
                                ProfileManager::CreateCallback(),
                                ProfileMetrics::ICON_AVATAR_BUBBLE);
      EXPECT_TRUE(supervised);
      target_browser = chrome::FindAnyBrowser(supervised, true);
    }
    OpenProfileMenuView(target_browser);
  }

 protected:
  void OpenProfileMenuView(Browser* browser) {
    ProfileMenuView::close_on_deactivate_for_testing_ = false;
    OpenProfileMenuViews(browser);

    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(ProfileMenuView::IsShowing());
  }

  void OpenProfileMenuViews(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    views::View* button = browser_view->toolbar()->GetAvatarToolbarButton();
    DCHECK(button);

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(e);
  }

  AvatarMenu* GetProfileMenuViewAvatarMenu() {
    return current_profile_bubble()->avatar_menu_.get();
  }

  void ClickProfileMenuViewLockButton() {
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    current_profile_bubble()->ButtonPressed(
        current_profile_bubble()->lock_button_, e);
  }

  // Access the registry that has been prepared with at least one extension.
  extensions::ExtensionRegistry* GetPreparedRegistry(Profile* signed_in) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(signed_in);
    const size_t initial_num_extensions = registry->enabled_extensions().size();
    const extensions::Extension* ext = LoadExtension(
        test_data_dir_.AppendASCII("app"));
    EXPECT_TRUE(ext);
    EXPECT_EQ(initial_num_extensions + 1,
              registry->enabled_extensions().size());
    EXPECT_EQ(0U, registry->blocked_extensions().size());
    return registry;
  }

  ProfileMenuView* current_profile_bubble() {
    return static_cast<ProfileMenuView*>(
        ProfileMenuView::GetBubbleForTesting());
  }

  int GetDiceSigninPromoShowCount() {
    return current_profile_bubble()->GetDiceSigninPromoShowCount();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewExtensionsTest);
};

// TODO(crbug.com/932818): Remove this class after
// |kAutofillEnableToolbarStatusChip| is cleaned up. Otherwise we need it
// because the toolbar is init-ed before each test is set up. Thus need to
// enable the feature in the general browsertest SetUp().
class ProfileMenuViewExtensionsParamTest
    : public ProfileMenuViewExtensionsTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ProfileMenuViewExtensionsParamTest()
      : ProfileMenuViewExtensionsTest() {}
  ~ProfileMenuViewExtensionsParamTest() override {}

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          autofill::features::kAutofillEnableToolbarStatusChip);
    }

    ProfileMenuViewExtensionsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ClickSigninButton) {
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));

  views::ButtonListener* bubble = current_profile_bubble();
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  base::UserActionTester tester;
  views::FocusManager* focus_manager =
      current_profile_bubble()->GetFocusManager();

  // Advance the focus to the first button in the menu, i.e. the signin button.
  focus_manager->AdvanceFocus(/*reverse=*/false);
  views::Button* signin_button =
      static_cast<views::Button*>(focus_manager->GetFocusedView());
  // Click the button.
  bubble->ButtonPressed(signin_button, event);
  EXPECT_EQ(1, tester.GetActionCount("Signin_Signin_FromAvatarBubbleSignin"));
}

// Make sure nothing bad happens when the browser theme changes while the
// ProfileMenuView is visible. Regression test for crbug.com/737470
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ThemeChanged) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile())));
  theme_change_observer.Wait();

  EXPECT_TRUE(ProfileMenuView::IsShowing());
  current_profile_bubble()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ViewProfileUMA) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  base::HistogramTester histograms;
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown, 0);

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, LockProfile) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  AvatarMenu* menu = GetProfileMenuViewAvatarMenu();
  EXPECT_FALSE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  ClickProfileMenuViewLockButton();
  EXPECT_TRUE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  if (!BrowserList::GetInstance()->empty())
    ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       LockProfileBlockExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  ClickProfileMenuViewLockButton();

  if (!BrowserList::GetInstance()->empty())
    ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the ExtensionService is blocked.
  ASSERT_EQ(1U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       LockProfileNoBlockOtherProfileExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());
  const size_t total_enabled_extensions = registry->enabled_extensions().size();

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  // Create a different profile and then lock it.
  Profile* signed_in = CreateTestingProfile("signed_in");
  SetupProfilesForLock(signed_in);
  extensions::ExtensionSystem::Get(signed_in)->InitForRegularProfile(
      true /* extensions_enabled */);
  Browser* browser_to_lock = CreateBrowser(signed_in);
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser_to_lock));
  ClickProfileMenuViewLockButton();

  if (1U != BrowserList::GetInstance()->size())
    ui_test_utils::WaitForBrowserToClose(browser_to_lock);
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the first profile's extensions are not blocked.
  ASSERT_EQ(total_enabled_extensions, registry->enabled_extensions().size());
  ASSERT_EQ(0U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

// Profile chooser view should close when a tab is added.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnTadAdded) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_strip->active_index());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when active tab is changed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabChanged) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->ActivateTabAt(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when active tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when the last tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnLastTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Shows a non-signed in profile with no others.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Shows a signed in profile with no others.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_SignedIn) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| with three different profiles.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_MultiProfile) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| during a Guest browsing session.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest, InvokeUi_Guest) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| during a Guest browsing session when the DICE
// flag is enabled.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_DiceGuest) {
  ScopedAccountConsistencyDice scoped_dice;
  ShowAndVerifyUi();
}

// Shows the manage account link, which appears when account consistency is
// enabled for signed-in accounts.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_ManageAccountLink) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| from a signed-in account that has a supervised
// user profile attached.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_SupervisedOwner) {
  ShowAndVerifyUi();
}

// Crashes because account consistency changes:  http://crbug.com/820390
// Shows the |ProfileMenuView| when a supervised user is the active profile.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       DISABLED_InvokeUi_SupervisedUser) {
  ShowAndVerifyUi();
}

// Open the profile chooser to increment the Dice sign-in promo show counter
// below the threshold.
// TODO(https://crbug.com/862573): Re-enable when no longer failing when
// is_chrome_branded is true.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_IncrementDiceSigninPromoShowCounter \
  DISABLED_IncrementDiceSigninPromoShowCounter
#else
#define MAYBE_IncrementDiceSigninPromoShowCounter \
  IncrementDiceSigninPromoShowCounter
#endif
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       MAYBE_IncrementDiceSigninPromoShowCounter) {
  ScopedAccountConsistencyDice scoped_dice;
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 7);
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 8);
}

// The DICE sync illustration is shown only the first 10 times. This test
// ensures that the profile chooser is shown correctly above this threshold.
// TODO(https://crbug.com/862573): Re-enable when no longer failing when
// is_chrome_branded is true.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_DiceSigninPromoWithoutIllustration \
  DISABLED_DiceSigninPromoWithoutIllustration
#else
#define MAYBE_DiceSigninPromoWithoutIllustration \
  DiceSigninPromoWithoutIllustration
#endif
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       MAYBE_DiceSigninPromoWithoutIllustration) {
  ScopedAccountConsistencyDice scoped_dice;
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 10);
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 11);
}

// Verify there is no crash when the chooser is used to display a signed-in
// profile with an empty username.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, SignedInNoUsername) {
  AddAccountToProfile(browser()->profile(), "");
  OpenProfileMenuView(browser());
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMenuViewExtensionsParamTest,
                         ::testing::Bool());

/*- - - - - - - - - - Profile menu revamp browser tests - - - - - - - - - - -*/

// This class is used to test the existence, the correct order and the call to
// the correct action of the buttons in the profile menu. This is done by
// advancing the focus to each button and simulating a click. It is expected
// that each button records a histogram sample from
// |ProfileMenuView::ActionableItem|.
//
// Subclasses have to implement |GetExpectedActionableItemAtIndex|. The test
// itself should contain the setup and a call to |RunTest|. Example test suite
// instantiation:
//
// class ProfileMenuClickTest_WithPrimaryAccount : public ProfileMenuClickTest {
//   ...
//   ProfileMenuView::ActionableItem GetExpectedActionableItemAtIndex(
//      size_t index) override {
//     return ...;
//   }
// };
//
// IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_WithPrimaryAccount,
//  SetupAndRunTest) {
//   ... /* setup primary account */
//   RunTest();
// }
//
// INSTANTIATE_TEST_SUITE_P(
//   ,
//   ProfileMenuClickTest_WithPrimaryAccount,
//   ::testing::Range(0, num_of_actionable_items));
//
class ProfileMenuClickTest : public InProcessBrowserTest,
                             public testing::WithParamInterface<size_t> {
 public:
  ProfileMenuClickTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kProfileMenuRevamp);
  }

  virtual ProfileMenuView::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) = 0;

  // This should be called in the test body.
  void RunTest() {
    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
    AdvanceFocus(/*count=*/GetParam() + 1);
    ASSERT_TRUE(GetFocusedItem());
    ClickFocusedItem();

    histogram_tester_.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        GetExpectedActionableItemAtIndex(GetParam()), /*count=*/1);
  }

 private:
  void OpenProfileMenu() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* avatar_button =
        browser_view->toolbar()->GetAvatarToolbarButton();
    DCHECK(avatar_button);

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    avatar_button->OnMousePressed(e);

    ASSERT_TRUE(ProfileMenuView::IsShowing());
  }

  void AdvanceFocus(int count) {
    for (int i = 0; i < count; i++)
      profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  }

  views::View* GetFocusedItem() {
    return profile_menu_view()->GetFocusManager()->GetFocusedView();
  }

  void ClickFocusedItem() {
    // Simulate a mouse click. Note: Buttons are either fired when pressed or
    // when released, so the corresponding methods need to be called.
    GetFocusedItem()->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    GetFocusedItem()->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  views::View* profile_menu_view() {
    return ProfileMenuViewBase::GetBubbleForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickTest);
};

class ProfileMenuClickTest_MultipleProfiles : public ProfileMenuClickTest {
 public:
  // List of actionable items in the correct order as they appear in the menu.
  // If a new button is added to the menu, it should also be added to this list.
  static constexpr ProfileMenuView::ActionableItem kOrderedActionableItems[6] =
      {ProfileMenuView::ActionableItem::kPasswordsButton,
       ProfileMenuView::ActionableItem::kCreditCardsButton,
       ProfileMenuView::ActionableItem::kAddressesButton,
       ProfileMenuView::ActionableItem::kOtherProfileButton,
       ProfileMenuView::ActionableItem::kOtherProfileButton,
       // The first button is added again to finish the cycle and test that
       // there are no other buttons at the end.
       ProfileMenuView::ActionableItem::kPasswordsButton};

  ProfileMenuClickTest_MultipleProfiles() = default;

  ProfileMenuView::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) override {
    return kOrderedActionableItems[index];
  }

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickTest_MultipleProfiles);
};

// static
constexpr ProfileMenuView::ActionableItem
    ProfileMenuClickTest_MultipleProfiles::kOrderedActionableItems[];

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_MultipleProfiles, SetupAndRunTest) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
  CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileMenuClickTest_MultipleProfiles,
    ::testing::Range(
        size_t(0),
        base::size(
            ProfileMenuClickTest_MultipleProfiles::kOrderedActionableItems)));

class ProfileMenuClickTest_WithPrimaryAccount : public ProfileMenuClickTest {
 public:
  // List of actionable items in the correct order as they appear in the menu.
  // If a new button is added to the menu, it should also be added to this list.
  static constexpr ProfileMenuView::ActionableItem kOrderedActionableItems[4] =
      {ProfileMenuView::ActionableItem::kPasswordsButton,
       ProfileMenuView::ActionableItem::kCreditCardsButton,
       ProfileMenuView::ActionableItem::kAddressesButton,
       // The first button is added again to finish the cycle and test that
       // there are no other buttons at the end.
       ProfileMenuView::ActionableItem::kPasswordsButton};

  ProfileMenuClickTest_WithPrimaryAccount() = default;

  ProfileMenuView::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) override {
    return kOrderedActionableItems[index];
  }

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickTest_WithPrimaryAccount);
};

// static
constexpr ProfileMenuView::ActionableItem
    ProfileMenuClickTest_WithPrimaryAccount::kOrderedActionableItems[];

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_WithPrimaryAccount,
                       SetupAndRunTest) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  ASSERT_FALSE(identity_manager->HasPrimaryAccount());
  signin::MakePrimaryAccountAvailable(identity_manager, "primary@example.com");

  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileMenuClickTest_WithPrimaryAccount,
    ::testing::Range(
        size_t(0),
        base::size(
            ProfileMenuClickTest_WithPrimaryAccount::kOrderedActionableItems)));
