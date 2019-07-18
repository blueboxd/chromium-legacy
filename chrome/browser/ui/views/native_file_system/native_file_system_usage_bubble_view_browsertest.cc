// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "third_party/blink/public/common/features.h"

class NativeFileSystemUsageBubbleViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
    DialogBrowserTest::SetUp();
  }

  void ShowUi(const std::string& name) override {
    NativeFileSystemUsageBubbleView::Usage usage;
    if (name == "SingleWritableFile") {
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
    } else if (name == "SingleWritableFolder") {
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    } else if (name == "SingleReadableFolder") {
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Images"));
    } else if (name == "default") {
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/home/me/Images"));
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Processing"));
      usage.readable_directories.emplace_back(FILE_PATH_LITERAL("Assets"));
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/index.html"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("view.js"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("README.md"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    } else {
      NOTREACHED() << "Unimplemented test: " << name;
    }

    NativeFileSystemUsageBubbleView::ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), kTestOrigin,
        std::move(usage));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemUsageBubbleViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemUsageBubbleViewTest,
                       InvokeUi_SingleWritableFile) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemUsageBubbleViewTest,
                       InvokeUi_SingleWritableFolder) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemUsageBubbleViewTest,
                       InvokeUi_SingleReadableFolder) {
  ShowAndVerifyUi();
}
