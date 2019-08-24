// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

// Fake ui::SelectFileDialog that selects one or more pre-determined files.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(std::vector<base::FilePath> result,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)) {}

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (result_.size() == 1)
      listener_->FileSelected(result_[0], 0, params);
    else
      listener_->MultiFilesSelected(result_, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;
  std::vector<base::FilePath> result_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(std::vector<base::FilePath> result)
      : result_(std::move(result)) {}
  ~FakeSelectFileDialogFactory() override = default;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(result_, listener, std::move(policy));
  }

 private:
  std::vector<base::FilePath> result_;
};

}  // namespace

// End-to-end tests for the native file system API. Among other things, these
// test the integration between usage of the Native File System API and the
// various bits of UI and permissions checks implemented in the chrome layer.
class NativeFileSystemBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);

    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_EQ(int{contents.size()},
              base::WriteFile(result, contents.data(), contents.size()));
    return result;
  }

  bool IsUsageIndicatorVisible() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* icon_view = browser_view->toolbar_button_provider()
                          ->GetOmniboxPageActionIconContainerView()
                          ->GetPageActionIconView(
                              PageActionIconType::kNativeFileSystemAccess);
    return icon_view && icon_view->GetVisible();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemBrowserTest, SaveFile) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(IsUsageIndicatorVisible());

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.chooseFileSystemEntries("
                            "      {type: 'saveFile'});"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  EXPECT_TRUE(IsUsageIndicatorVisible())
      << "A save file dialog implicitly grants write access, so usage "
         "indicator should be visible.";

  EXPECT_EQ(
      int{file_contents.size()},
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemBrowserTest, OpenFile) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NativeFileSystemPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(PermissionAction::GRANTED);

  EXPECT_FALSE(IsUsageIndicatorVisible());

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.chooseFileSystemEntries("
                            "      {type: 'openFile'});"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Should be writable yet, so no usage indicator.
  EXPECT_FALSE(IsUsageIndicatorVisible());

  EXPECT_EQ(
      int{file_contents.size()},
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  // Should have prompted for and received write access, so usage indicator
  // should now be visible.
  EXPECT_TRUE(IsUsageIndicatorVisible());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

// TODO(mek): Add more end-to-end test including other bits of UI.
