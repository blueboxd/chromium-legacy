// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/hats/hats_web_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class HatsBubbleTest : public DialogBrowserTest {
 public:
  HatsBubbleTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(browser()->is_type_normal());
    BrowserView::GetBrowserViewForBrowser(InProcessBrowserTest::browser())
        ->ShowHatsBubble("test_site_id");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsBubbleTest);
};

class TestHatsWebDialog : public HatsWebDialog {
 public:
  TestHatsWebDialog(Browser* browser,
                    const base::TimeDelta& timeout,
                    const GURL& url)
      : HatsWebDialog(browser, "fake_id_not_used"),
        loading_timeout_(timeout),
        content_url_(url) {}

  // ui::WebDialogDelegate implementation.
  GURL GetDialogContentURL() const override {
    if (content_url_.is_valid()) {
      // When we have a valid overridden url, use it instead.
      return content_url_;
    }
    return HatsWebDialog::GetDialogContentURL();
  }

  MOCK_METHOD0(OnWebContentsFinishedLoad, void());
  MOCK_METHOD0(OnLoadTimedOut, void());

 private:
  const base::TimeDelta ContentLoadingTimeout() const override {
    return loading_timeout_;
  }

  base::TimeDelta loading_timeout_;
  GURL content_url_;
};

class HatsWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  TestHatsWebDialog* Create(Browser* browser,
                            const base::TimeDelta& timeout,
                            const GURL& url = GURL()) {
    auto* hats_dialog = new TestHatsWebDialog(browser, timeout, url);
    hats_dialog->CreateWebDialog(browser);
    return hats_dialog;
  }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(HatsBubbleTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

// Test time out of preloading works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, Timeout) {
  TestHatsWebDialog* dialog = Create(browser(), base::TimeDelta());
  EXPECT_CALL(*dialog, OnLoadTimedOut).Times(1);
}

// Test preloading content works.
IN_PROC_BROWSER_TEST_F(HatsWebDialogBrowserTest, ContentPreloading) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(test_data_dir.AppendASCII("simple.html"),
                                       &contents));
  }

  TestHatsWebDialog* dialog =
      Create(browser(), base::TimeDelta::FromSeconds(100),
             GURL("data:text/html;charset=utf-8," + contents));
  base::RunLoop run_loop;
  EXPECT_CALL(*dialog, OnWebContentsFinishedLoad)
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}
