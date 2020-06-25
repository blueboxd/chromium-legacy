// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/floc_blocklist_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/federated_learning/floc_blocklist_service.h"
#include "components/federated_learning/floc_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
  return true;
}

// This class monitors the OnBlocklistFileReady method calls.
class MockFlocBlocklistService
    : public federated_learning::FlocBlocklistService {
 public:
  MockFlocBlocklistService() = default;

  MockFlocBlocklistService(const MockFlocBlocklistService&) = delete;
  MockFlocBlocklistService& operator=(const MockFlocBlocklistService&) = delete;

  ~MockFlocBlocklistService() override = default;

  void OnBlocklistFileReady(const base::FilePath& file_path) override {
    file_paths_.push_back(file_path);
  }

  const std::vector<base::FilePath>& file_paths() const { return file_paths_; }

 private:
  std::vector<base::FilePath> file_paths_;
};

}  //  namespace

class FlocBlocklistComponentInstallerTest : public PlatformTest {
 public:
  FlocBlocklistComponentInstallerTest() = default;

  FlocBlocklistComponentInstallerTest(
      const FlocBlocklistComponentInstallerTest&) = delete;
  FlocBlocklistComponentInstallerTest& operator=(
      const FlocBlocklistComponentInstallerTest&) = delete;

  ~FlocBlocklistComponentInstallerTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());

    auto test_floc_blocklist_service =
        std::make_unique<MockFlocBlocklistService>();
    test_floc_blocklist_service->SetBackgroundTaskRunnerForTesting(
        base::SequencedTaskRunnerHandle::Get());

    test_floc_blocklist_service_ = test_floc_blocklist_service.get();

    TestingBrowserProcess::GetGlobal()->SetFlocBlocklistService(
        std::move(test_floc_blocklist_service));
    policy_ = std::make_unique<FlocBlocklistComponentInstallerPolicy>(
        test_floc_blocklist_service_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetFlocBlocklistService(nullptr);
    PlatformTest::TearDown();
  }

  MockFlocBlocklistService* service() { return test_floc_blocklist_service_; }

  void WriteStringToFile(const std::string& data, const base::FilePath& path) {
    ASSERT_EQ(base::WriteFile(path, data.data(), data.length()),
              static_cast<int32_t>(data.length()));
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  void CreateTestFlocBlocklist(const std::string& contents) {
    base::FilePath file_path =
        component_install_dir().Append(federated_learning::kBlocklistFileName);
    ASSERT_NO_FATAL_FAILURE(WriteStringToFile(contents, file_path));
  }

  void LoadFlocBlocklist(const std::string& content_version,
                         int format_version) {
    auto manifest = std::make_unique<base::DictionaryValue>();
    manifest->SetInteger(federated_learning::kManifestBlocklistFormatKey,
                         format_version);

    if (!policy_->VerifyInstallation(*manifest, component_install_dir()))
      return;

    policy_->ComponentReady(base::Version(content_version),
                            component_install_dir(), std::move(manifest));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir component_install_dir_;
  std::unique_ptr<FlocBlocklistComponentInstallerPolicy> policy_;
  MockFlocBlocklistService* test_floc_blocklist_service_ = nullptr;
};

TEST_F(FlocBlocklistComponentInstallerTest, TestComponentRegistration) {
  auto component_updater =
      std::make_unique<component_updater::MockComponentUpdateService>();

  base::RunLoop run_loop;
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(QuitMessageLoop(&run_loop));

  RegisterFlocBlocklistComponent(component_updater.get(), service());
  run_loop.Run();
}

TEST_F(FlocBlocklistComponentInstallerTest, LoadBlocklist) {
  ASSERT_TRUE(service());

  std::string contents = "abcd";
  ASSERT_NO_FATAL_FAILURE(CreateTestFlocBlocklist(contents));
  ASSERT_NO_FATAL_FAILURE(LoadFlocBlocklist(
      "1.0.0", federated_learning::kCurrentBlocklistFormatVersion));

  ASSERT_EQ(service()->file_paths().size(), 1u);

  // Assert that the file path is the concatenation of |component_install_dir_|
  // and |kBlocklistFileName|, which implies that the |version| argument has no
  // impact. In reality, though, the |component_install_dir_| and the |version|
  // should always match.
  ASSERT_EQ(service()->file_paths()[0].AsUTF8Unsafe(),
            component_install_dir()
                .Append(federated_learning::kBlocklistFileName)
                .AsUTF8Unsafe());

  std::string actual_contents;
  ASSERT_TRUE(
      base::ReadFileToString(service()->file_paths()[0], &actual_contents));
  EXPECT_EQ(actual_contents, contents);
}

TEST_F(FlocBlocklistComponentInstallerTest, UnsupportedFormatVersionIgnored) {
  ASSERT_TRUE(service());
  const std::string contents = "future stuff";
  ASSERT_NO_FATAL_FAILURE(CreateTestFlocBlocklist(contents));
  ASSERT_NO_FATAL_FAILURE(LoadFlocBlocklist(
      "1.0.0", federated_learning::kCurrentBlocklistFormatVersion + 1));
  EXPECT_EQ(service()->file_paths().size(), 0u);
}

}  // namespace component_updater
