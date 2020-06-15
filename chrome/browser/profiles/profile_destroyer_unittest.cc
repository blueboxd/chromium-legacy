// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"

class ProfileDestroyerTest : public BrowserWithTestWindowTest {
 public:
  ProfileDestroyerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    GetOriginalProfile()->SetProfileDestructionObserver(
        base::BindOnce(&ProfileDestroyerTest::SetOriginalProfileDestroyed,
                       base::Unretained(this)));
  }

  TestingProfile* GetOriginalProfile() { return GetProfile(); }

  TestingProfile* GetIncognitoProfile() {
    if (!incognito_profile_) {
      TestingProfile::Builder builder;
      incognito_profile_ = builder.BuildIncognito(GetOriginalProfile());
      incognito_profile_->SetProfileDestructionObserver(
          base::BindOnce(&ProfileDestroyerTest::SetIncognitoProfileDestroyed,
                         base::Unretained(this)));
    }
    return incognito_profile_;
  }

  void SetOriginalProfileDestroyed() { original_profile_destroyed_ = true; }
  void SetIncognitoProfileDestroyed() { incognito_profile_destroyed_ = true; }

  bool IsOriginalProfileDestroyed() { return original_profile_destroyed_; }
  bool IsIncognitoProfileDestroyed() { return incognito_profile_destroyed_; }

  // Creates a render process host based on a new site instance given the
  // |profile| and mark it as used.
  std::unique_ptr<content::RenderProcessHost> CreatedRendererProcessHost(
      Profile* profile) {
    site_instances_.emplace_back(content::SiteInstance::Create(profile));

    std::unique_ptr<content::RenderProcessHost> render_process_host;
    render_process_host.reset(site_instances_.back()->GetProcess());
    render_process_host->SetIsUsed();
    EXPECT_NE(render_process_host.get(), nullptr);

    return render_process_host;
  }

 protected:
  bool original_profile_destroyed_{false};
  bool incognito_profile_destroyed_{false};
  TestingProfile* incognito_profile_{nullptr};

  std::vector<scoped_refptr<content::SiteInstance>> site_instances_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyerTest);
};

// Expect immediate incognito profile destruction when no pending renderer
// process host exists.
TEST_F(ProfileDestroyerTest, ImmediateIncognitoProfileDestruction) {
  TestingProfile* incognito_profile = GetIncognitoProfile();

  // Destroying the regular browser does not result in destruction of regular
  // profile and hence should not destroy the incognito profile.
  set_browser(nullptr);
  EXPECT_FALSE(IsOriginalProfileDestroyed());
  EXPECT_FALSE(IsIncognitoProfileDestroyed());

  // Ask for destruction of Incognito profile, and expect immediate destruction.
  ProfileDestroyer::DestroyProfileWhenAppropriate(incognito_profile);
  EXPECT_TRUE(IsIncognitoProfileDestroyed());
}

// Expect pending renderer process hosts delay incognito profile destruction.
TEST_F(ProfileDestroyerTest, DelayedIncognitoProfileDestruction) {
  TestingProfile* incognito_profile = GetIncognitoProfile();

  // Create two render process hosts.
  std::unique_ptr<content::RenderProcessHost> render_process_host1 =
      CreatedRendererProcessHost(incognito_profile);
  std::unique_ptr<content::RenderProcessHost> render_process_host2 =
      CreatedRendererProcessHost(incognito_profile);

  // Ask for destruction of Incognito profile, but expect it to be delayed.
  ProfileDestroyer::DestroyProfileWhenAppropriate(incognito_profile);
  EXPECT_FALSE(IsIncognitoProfileDestroyed());

  // Destroy the first pending render process host, and expect it not to destroy
  // the incognito profile.
  render_process_host1.release()->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsIncognitoProfileDestroyed());

  // Destroy the other renderer process, and expect destruction of incognito
  // profile.
  render_process_host2.release()->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsIncognitoProfileDestroyed());
}

// TODO(https://crbug.com/1033903): Add tests for non-primary OTRs.
