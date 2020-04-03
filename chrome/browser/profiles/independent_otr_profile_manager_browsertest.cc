// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/independent_otr_profile_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

class ProfileDestructionWatcher : public ProfileObserver {
 public:
  ProfileDestructionWatcher() = default;
  ~ProfileDestructionWatcher() override = default;

  void Watch(Profile* profile) { observed_profiles_.Add(profile); }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    destroyed_ = true;
    observed_profiles_.Remove(profile);
  }

  bool destroyed() const { return destroyed_; }

 private:
  bool destroyed_ = false;
  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileDestructionWatcher);
};

// Waits for |browser| to be removed from BrowserList and then calls |callback|.
// This is used to ensure that the Browser object and its window are destroyed
// after a call to BrowserWindow::Close, since base::RunLoop::RunUntilIdle
// doesn't ensure this on Mac.
class BrowserRemovedWaiter final : public BrowserListObserver {
 public:
  BrowserRemovedWaiter(Browser* browser, base::OnceClosure callback)
      : browser_(browser), callback_(std::move(callback)) {
    BrowserList::AddObserver(this);
  }
  ~BrowserRemovedWaiter() override = default;

  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      BrowserList::RemoveObserver(this);
      std::move(callback_).Run();
    }
  }

 private:
  Browser* browser_;
  base::OnceClosure callback_;
};

void OriginalProfileNeverDestroyed(Profile* profile) {
  FAIL()
      << "Original profile unexpectedly destroyed before dependent OTR profile";
}

// This class acts as an owner of an OTRProfileRegistration and deletes the
// registration when it is notified that the original profile is being
// destroyed.  This is the minimum behavior expected by owners of
// OTRProfileRegistration.
class RegistrationOwner {
 public:
  // |profile| is an original Profile from which we are creating a registered
  // OTR profile.  |this| will own the resulting OTR profile registration.
  RegistrationOwner(IndependentOTRProfileManager* manager, Profile* profile)
      : otr_profile_registration_(manager->CreateFromOriginalProfile(
            profile,
            base::BindOnce(&RegistrationOwner::OriginalProfileDestroyed,
                           base::Unretained(this)))) {}

  Profile* profile() const { return otr_profile_registration_->profile(); }

 private:
  void OriginalProfileDestroyed(Profile* profile) {
    DCHECK(profile == otr_profile_registration_->profile());
    otr_profile_registration_.reset();
  }

  std::unique_ptr<IndependentOTRProfileManager::OTRProfileRegistration>
      otr_profile_registration_;
};

}  // namespace

class IndependentOTRProfileManagerTest : public InProcessBrowserTest {
 protected:
  void EnableProfileHelperTestSettings() {
    chromeos::ProfileHelper::Get()->SetProfileToUserForTestingEnabled(true);
    chromeos::ProfileHelper::Get()->SetAlwaysReturnPrimaryUserForTesting(true);
  }

  void SafelyDestroyOriginalProfile() {
    if (!original_profile_) {
      return;
    }

    // We are about to destroy a profile. In production that will only happen
    // as part of the destruction of BrowserProcess's ProfileManager. This
    // happens in PostMainMessageLoopRun(). This means that to have this test
    // represent production we have to make sure that no tasks are pending on
    // the main thread before we destroy the profile. We also would need to
    // prohibit the posting of new tasks on the main thread as in production the
    // main thread's message loop will not be accepting them. We fallback on
    // flushing as many runners as possible here to avoid the posts coming from
    // any of them.
    content::RunAllTasksUntilIdle();

    original_profile_.reset();

    // Pending tasks on the ThreadPool initiated by |profile_| could depend on
    // |temp_dir_|. We need to let them complete before |temp_dir| goes out of
    // scope.
    content::RunAllTasksUntilIdle();
  }

  IndependentOTRProfileManager* manager_ =
      IndependentOTRProfileManager::GetInstance();

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Profile> original_profile_;
};

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest, CreateAndDestroy) {
  ProfileDestructionWatcher watcher;
  {
    auto profile_registration = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile = profile_registration->profile();

    ASSERT_NE(browser()->profile(), otr_profile);
    EXPECT_NE(browser()->profile()->GetOffTheRecordProfile(), otr_profile);
    EXPECT_TRUE(otr_profile->IsOffTheRecord());

    watcher.Watch(otr_profile);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       CreateTwoFromSameProfile) {
  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;
  {
    auto profile_registration1 = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile1 = profile_registration1->profile();

    auto profile_registration2 = manager_->CreateFromOriginalProfile(
        browser()->profile(), base::BindOnce(&OriginalProfileNeverDestroyed));
    auto* otr_profile2 = profile_registration2->profile();

    ASSERT_NE(otr_profile1, otr_profile2);

    watcher1.Watch(otr_profile1);
    watcher2.Watch(otr_profile2);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(watcher1.destroyed());
  EXPECT_TRUE(watcher2.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       OriginalProfileDestroyedFirst) {
  ProfileDestructionWatcher watcher;

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if defined(OS_CHROMEOS)
  EnableProfileHelperTestSettings();
#endif
  original_profile_ = Profile::CreateProfile(temp_dir_.GetPath(), nullptr,
                                             Profile::CREATE_MODE_SYNCHRONOUS);
  ASSERT_TRUE(original_profile_);
  auto profile_owner = RegistrationOwner(manager_, original_profile_.get());
  auto* otr_profile = profile_owner.profile();

  ASSERT_NE(original_profile_.get(), otr_profile);
  EXPECT_NE(original_profile_->GetOffTheRecordProfile(), otr_profile);

  watcher.Watch(otr_profile);

  SafelyDestroyOriginalProfile();

  // |original_profile_| being destroyed should trigger the dependent OTR
  // profile to be destroyed.
  EXPECT_TRUE(watcher.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest,
                       OriginalProfileDestroyedFirstTwoOTR) {
  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if defined(OS_CHROMEOS)
  EnableProfileHelperTestSettings();
#endif
  original_profile_ = Profile::CreateProfile(temp_dir_.GetPath(), nullptr,
                                             Profile::CREATE_MODE_SYNCHRONOUS);
  ASSERT_TRUE(original_profile_);
  auto profile_owner1 = RegistrationOwner(manager_, original_profile_.get());
  auto* otr_profile1 = profile_owner1.profile();

  auto profile_owner2 = RegistrationOwner(manager_, original_profile_.get());
  auto* otr_profile2 = profile_owner2.profile();

  watcher1.Watch(otr_profile1);
  watcher2.Watch(otr_profile2);

  SafelyDestroyOriginalProfile();

  // |original_profile_| being destroyed should trigger the dependent OTR
  // profiles to be destroyed.
  EXPECT_TRUE(watcher1.destroyed());
  EXPECT_TRUE(watcher2.destroyed());
}

IN_PROC_BROWSER_TEST_F(IndependentOTRProfileManagerTest, Notifications) {
  // Create the OTR profile.
  content::WindowedNotificationObserver profile_created_observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  auto* profile = browser()->profile();
  auto profile_registration = manager_->CreateFromOriginalProfile(
      profile, base::BindOnce(&OriginalProfileNeverDestroyed));
  profile_created_observer.Wait();

  // Verify the received notification.
  auto* otr_profile = profile_registration->profile();
  EXPECT_EQ(profile_created_observer.source(),
            content::Source<Profile>(otr_profile));
  EXPECT_FALSE(profile->HasOffTheRecordProfile());
  EXPECT_TRUE(otr_profile->IsOffTheRecord());
  EXPECT_TRUE(otr_profile->IsIndependentOffTheRecordProfile());
}
