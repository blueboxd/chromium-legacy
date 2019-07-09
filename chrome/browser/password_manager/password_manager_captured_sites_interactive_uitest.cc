// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/test_utils.h"

namespace {

constexpr base::TimeDelta kWaitForSaveFallbackInterval =
    base::TimeDelta::FromSeconds(5);

struct TestParams {
  std::string scenarioDir;
  std::string siteName;
};

// Return path to the Password Manager captured sites test root directory. The
// directory contains subdirectories for different password manager test
// scenarios. The test scenario subdirectories contain site capture files
// and test recipe replay files.
base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("password")
        .AppendASCII("captured_sites");
  }

  ADD_FAILURE() << "Unable to obtain the Chromium src directory!";
  src_dir.clear();
  return src_dir;
}

// Iterate through Password Manager's Web Page Replay capture file directory to
// look for captures sites and automation recipe files. Return a list of sites
// for which recipe-based testing is available.
std::vector<TestParams> GetCapturedSites() {
  std::vector<TestParams> sites;
  base::FileEnumerator sub_dirs(GetReplayFilesRootDirectory(), false,
                                base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir = sub_dirs.Next(); !dir.empty();
       dir = sub_dirs.Next()) {
    base::FileEnumerator capture_files(dir, false, base::FileEnumerator::FILES);
    for (base::FilePath file = capture_files.Next(); !file.empty();
         file = capture_files.Next()) {
      // If a site capture file is found, also look to see if the directory has
      // a corresponding recorded action recipe log file.
      // A site capture file has no extension. A recorded action recipe log file
      // has the '.test' extension.
      if (file.Extension().empty() &&
          base::PathExists(file.AddExtension(FILE_PATH_LITERAL(".test")))) {
        TestParams params;
        params.scenarioDir =
            captured_sites_test_utils::FilePathToUTF8(dir.BaseName().value());
        params.siteName =
            captured_sites_test_utils::FilePathToUTF8(file.BaseName().value());
        sites.push_back(params);
      }
    }
  }
  return sites;
}

struct GetParamAsString {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return base::StringPrintf("%s_%s", info.param.scenarioDir.c_str(),
                              info.param.siteName.c_str());
  }
};

}  // namespace

namespace password_manager {

using autofill::test::ServerCacheReplayer;
using autofill::test::ServerUrlLoader;

// Harness for running password manager scenarios on captured real-world sites.
// Test params:
//  - string Recipe: the name of the captured site file and the test recipe
//        file.
class CapturedSitesPasswordManagerBrowserTest
    : public InProcessBrowserTest,
      public captured_sites_test_utils::
          TestRecipeReplayChromeFeatureActionExecutor,
      public ::testing::WithParamInterface<TestParams> {
 public:
  // TestRecipeReplayChromeFeatureActionExecutor:
  bool AddCredential(const std::string& origin,
                     const std::string& username,
                     const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());
    autofill::PasswordForm signin_form;
    signin_form.origin = GURL(origin);
    signin_form.signon_realm = origin;
    signin_form.password_value = base::ASCIIToUTF16(password);
    signin_form.username_value = base::ASCIIToUTF16(username);
    password_store->AddLogin(signin_form);
    return true;
  }

  bool SavePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsSavePromptAvailable()) {
      bubble_observer.AcceptSavePrompt();
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Save Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Save Password prompt!";
    return false;
  }

  bool UpdatePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsUpdatePromptAvailable()) {
      const autofill::PasswordForm& pending_credentials =
          ManagePasswordsUIController::FromWebContents(WebContents())
              ->GetPendingPassword();
      bubble_observer.AcceptUpdatePrompt(pending_credentials);
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Update Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Update Password prompt!";
    return false;
  }

  bool WaitForSaveFallback() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.WaitForFallbackForSaving(kWaitForSaveFallbackInterval))
      return true;
    ADD_FAILURE() << "Chrome did not show the save fallback icon!";
    return false;
  }

  bool HasChromeShownSavePasswordPrompt() override {
    BubbleObserver bubble_observer(WebContents());
    return bubble_observer.IsSavePromptShownAutomatically();
  }

  bool HasChromeStoredCredential(const std::string& origin,
                                 const std::string& username,
                                 const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());

    auto found = password_store->stored_passwords().find(origin);
    if (password_store->stored_passwords().end() == found) {
      return false;
    }

    const std::vector<autofill::PasswordForm>& passwords_vector = found->second;
    for (auto it = passwords_vector.begin(); it != passwords_vector.end();
         ++it) {
      if (base::ASCIIToUTF16(username) == it->username_value &&
          base::ASCIIToUTF16(password) == it->password_value) {
        return true;
      }
    }

    return false;
  }

 protected:
  CapturedSitesPasswordManagerBrowserTest() = default;
  ~CapturedSitesPasswordManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThreadAndGetNewTab(
        browser(), &web_contents_);
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();

    base::FilePath capture_file_path =
        GetReplayFilesRootDirectory()
            .AppendASCII(GetParam().scenarioDir.c_str())
            .AppendASCII(GetParam().siteName.c_str());
    SetServerUrlLoader(
        std::make_unique<ServerUrlLoader>(std::make_unique<ServerCacheReplayer>(
            capture_file_path,
            ServerCacheReplayer::kOptionFailOnInvalidJsonRecord)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  void SetServerUrlLoader(std::unique_ptr<ServerUrlLoader> server_url_loader) {
    server_url_loader_ = std::move(server_url_loader);
  }

  void TearDownOnMainThread() override {
    recipe_replayer()->Cleanup();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer  has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset(nullptr);
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

  content::WebContents* WebContents() {
    // return web_contents_;
    return web_contents_;
  }

 private:
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<ServerUrlLoader> server_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(CapturedSitesPasswordManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(CapturedSitesPasswordManagerBrowserTest, Recipe) {
  // Craft the capture file path.
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  base::FilePath capture_file_path =
      GetReplayFilesRootDirectory()
          .AppendASCII(GetParam().scenarioDir.c_str())
          .AppendASCII(GetParam().siteName.c_str());

  // Craft the recipe file path.
  base::FilePath recipe_file_path =
      GetReplayFilesRootDirectory()
          .AppendASCII(GetParam().scenarioDir.c_str())
          .AppendASCII(
              base::StringPrintf("%s.test", GetParam().siteName.c_str()));

  ASSERT_TRUE(
      recipe_replayer()->ReplayTest(capture_file_path, recipe_file_path));
}

INSTANTIATE_TEST_SUITE_P(,
                         CapturedSitesPasswordManagerBrowserTest,
                         testing::ValuesIn(GetCapturedSites()),
                         GetParamAsString());
}  // namespace password_manager
