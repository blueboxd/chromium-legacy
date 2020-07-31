// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpContentManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  void ChangeConfidentiality(content::WebContents* web_contents,
                             bool confidential) {
    manager_.OnConfidentialityChanged(web_contents, confidential);
  }

  void ChangeVisibility(content::WebContents* web_contents, bool visible) {
    if (visible) {
      web_contents->WasShown();
    } else {
      web_contents->WasHidden();
    }
    manager_.OnVisibilityChanged(web_contents, visible);
  }

  void DestroyWebContents(content::WebContents* web_contents) {
    manager_.OnWebContentsDestroyed(web_contents);
  }

  DlpContentManager manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpContentManagerTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());
}

TEST_F(DlpContentManagerTest, ConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  ChangeConfidentiality(web_contents.get(), /*confidential=*/true);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  DestroyWebContents(web_contents.get());
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());
}

TEST_F(DlpContentManagerTest, ConfidentialDataVisibilityChanged) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  ChangeConfidentiality(web_contents.get(), /*confidential=*/true);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  ChangeVisibility(web_contents.get(), /*visible=*/false);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  ChangeVisibility(web_contents.get(), /*visible=*/true);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  DestroyWebContents(web_contents.get());
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());
}

TEST_F(DlpContentManagerTest,
       TwoWebContentsVisibilityAndConfidentialityChanged) {
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  // WebContents 1 becomes confidential.
  ChangeConfidentiality(web_contents1.get(), /*confidential=*/true);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  // WebContents 2 is hidden.
  ChangeVisibility(web_contents2.get(), /*visible=*/false);
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  // WebContents 1 becomes non-confidential.
  ChangeConfidentiality(web_contents1.get(), /*confidential=*/false);
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  // WebContents 2 becomes confidential.
  ChangeConfidentiality(web_contents2.get(), /*confidential=*/true);
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());

  // WebContents 2 is visible.
  ChangeVisibility(web_contents2.get(), /*visible=*/true);
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_TRUE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_TRUE(manager_.IsConfidentialDataPresentOnScreen());

  DestroyWebContents(web_contents1.get());
  DestroyWebContents(web_contents2.get());
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents1.get()));
  EXPECT_FALSE(manager_.IsWebContentsConfidential(web_contents2.get()));
  EXPECT_FALSE(manager_.IsConfidentialDataPresentOnScreen());
}

}  // namespace policy
