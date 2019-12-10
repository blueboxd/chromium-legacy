// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

TEST(FileHandlerUtilsTest, GetFileExtensionsFromFileHandlers) {
  // Construct FileHandlerInfo vector with multiple file extensions.
  const std::vector<std::string> test_file_extensions = {"txt", "xls", "doc"};
  apps::FileHandlerInfo fhi1;
  apps::FileHandlerInfo fhi2;
  fhi1.extensions.insert(test_file_extensions[0]);
  fhi1.extensions.insert(test_file_extensions[1]);
  fhi2.extensions.insert(test_file_extensions[2]);
  std::vector<apps::FileHandlerInfo> file_handlers = {fhi1, fhi2};
  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(file_handlers);

  EXPECT_EQ(file_extensions.size(), test_file_extensions.size());
  for (const auto& test_file_extension : test_file_extensions) {
    EXPECT_TRUE(file_extensions.find(test_file_extension) !=
                file_extensions.end());
  }
}

TEST(FileHandlerUtilsTest, GetMimeTypesFromFileHandlers) {
  // Construct FileHandlerInfo vector with multiple mime types.
  const std::vector<std::string> test_mime_types = {
      "text/plain", "image/png", "application/vnd.my-app.file"};
  apps::FileHandlerInfo fhi1;
  apps::FileHandlerInfo fhi2;
  fhi1.types.insert(test_mime_types[0]);
  fhi1.types.insert(test_mime_types[1]);
  fhi2.types.insert(test_mime_types[2]);
  std::vector<apps::FileHandlerInfo> file_handlers = {fhi1, fhi2};
  std::set<std::string> mime_types =
      GetMimeTypesFromFileHandlers(file_handlers);

  EXPECT_EQ(mime_types.size(), test_mime_types.size());
  for (const auto& test_mime_type : test_mime_types) {
    EXPECT_TRUE(mime_types.find(test_mime_type) != mime_types.end());
  }
}

class FileHandlerManagerTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    features_.InitAndEnableFeature(blink::features::kFileHandlingAPI);

    registrar_ = std::make_unique<TestAppRegistrar>();
    file_handler_manager_ = std::make_unique<TestFileHandlerManager>(profile());

    file_handler_manager_->SetSubsystems(registrar_.get());
  }

  TestFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_.get();
  }

 private:
  std::unique_ptr<TestAppRegistrar> registrar_;
  std::unique_ptr<TestFileHandlerManager> file_handler_manager_;

  base::test::ScopedFeatureList features_;
};

TEST_F(FileHandlerManagerTest, FileHandlersAreNotAvailableUnlessEnabled) {
  const AppId app_id = "app-id";

  file_handler_manager().InstallFileHandler(
      app_id, GURL("https://app.site/handle-foo"), {".foo", "application/foo"},
      /*enable=*/false);

  file_handler_manager().InstallFileHandler(
      app_id, GURL("https://app.site/handle-bar"), {".bar", "application/bar"},
      /*enable=*/false);

  // File handlers are disabled by default.
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(nullptr, handlers);
  }

  // Ensure they can be enabled.
  file_handler_manager().EnableAndRegisterOsFileHandlers(app_id);
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(2u, handlers->size());
  }

  // Ensure they can be disabled.
  file_handler_manager().DisableAndUnregisterOsFileHandlers(app_id);

  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(nullptr, handlers);
  }
}

TEST_F(FileHandlerManagerTest, NoHandlersRegistered) {
  const AppId app_id = "app-id";

  // Returns nullopt when no file handlers are registered.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, NoLaunchFilesPassed) {
  const AppId app_id = "app-id";

  // Returns nullopt when no launch files are passed.
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {}));
}

TEST_F(FileHandlerManagerTest, SingleValidExtensionSingleExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo"});

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, SingleInvalidExtensionSingleExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo"});

  // Returns nullopt on single invalid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, SingleValidExtensionMultiExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo", ".bar"});

  // Matches on single valid extension for multi-extension handler.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, MultipleValidExtensions) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo", ".bar"});

  // Matches on multiple valid extensions for multi-extension handler.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(url, file_handler_manager().GetMatchingFileHandlerURL(
                     app_id, {path1, path2}));
}

TEST_F(FileHandlerManagerTest, PartialExtensionMatch) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo"});

  // Returns nullopt on partial extension match.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(base::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id, {path1, path2}));
}

TEST_F(FileHandlerManagerTest, SingleFileWithoutExtension) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo"});

  // Returns nullopt where a file has no extension.
  const base::FilePath path(FILE_PATH_LITERAL("file"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, FileWithoutExtensionAmongMultipleFiles) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url, {".foo"});

  // Returns nullopt where one file has no extension while others do.
  const base::FilePath path1(FILE_PATH_LITERAL("file"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(base::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id, {path1, path2}));
}

}  // namespace web_app
