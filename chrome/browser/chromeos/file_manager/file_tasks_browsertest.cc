// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/browser/entry_info.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/features.h"

using chromeos::default_web_apps::kMediaAppId;

namespace file_manager {
namespace file_tasks {
namespace {

// A list of file extensions (`/` delimited) representing a selection of files
// and the app expected to be the default to open these files.
struct Expectation {
  const char* file_extensions;
  const char* app_id;
};

// Verifies that a single default task expectation (i.e. the expected
// default app to open a given set of file extensions) matches the default
// task in a vector of task descriptors. Decrements the provided |remaining|
// integer to provide additional verification that this function is invoked
// an expected number of times (i.e. even if the callback could be invoked
// asynchronously).
void VerifyTasks(int* remaining,
                 Expectation expectation,
                 std::unique_ptr<std::vector<FullTaskDescriptor>> result) {
  ASSERT_TRUE(result) << expectation.file_extensions;

  auto default_task =
      std::find_if(result->begin(), result->end(),
                   [](const auto& task) { return task.is_default(); });

  ASSERT_TRUE(default_task != result->end()) << expectation.file_extensions;

  EXPECT_EQ(expectation.app_id, default_task->task_descriptor().app_id)
      << " for extension: " << expectation.file_extensions;

  // Verify no other task is set as default.
  EXPECT_EQ(1u,
            std::count_if(result->begin(), result->end(),
                          [](const auto& task) { return task.is_default(); }))
      << expectation.file_extensions;

  --*remaining;
}

class FileTasksBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    test::AddDefaultComponentExtensionsOnMainThread(browser()->profile());
    web_app::WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
  }

  // Tests that each of the passed expectations open by default in the expected
  // app.
  void TestExpectationsAgainstDefaultTasks(
      const std::vector<Expectation>& expectations) {
    int remaining = expectations.size();
    const base::FilePath prefix = base::FilePath().AppendASCII("file");

    for (const Expectation& test : expectations) {
      std::vector<extensions::EntryInfo> entries;
      std::vector<base::StringPiece> all_extensions =
          base::SplitStringPiece(test.file_extensions, "/",
                                 base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      for (base::StringPiece extension : all_extensions) {
        base::FilePath path = prefix.AddExtension(extension);
        // Fetching a mime type is part of the default app determination, but it
        // doesn't need to succeed.
        std::string mime_type;
        net::GetMimeTypeFromFile(path, &mime_type);
        entries.push_back({path, mime_type, false});
      }
      std::vector<GURL> file_urls{entries.size(), GURL()};

      // task_verifier callback is invoked synchronously from
      // FindAllTypesOfTasks.
      FindAllTypesOfTasks(browser()->profile(), entries, file_urls,
                          base::BindRepeating(&VerifyTasks, &remaining, test));
    }
    EXPECT_EQ(0, remaining);
  }
};

class FileTasksBrowserTestWithMediaApp : public FileTasksBrowserTest {
 public:
  FileTasksBrowserTestWithMediaApp() {
    // Enable Media App.
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMediaApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// List of single file default app expectations that we don't expect to change
// regardless of app flags. Changes to this test may have implications for file
// handling declarations in built-in app manifests, because logic in
// ChooseAndSetDefaultTask() treats handlers for extensions with a higher
// priority than handlers for mime types.
constexpr Expectation kUnchangedExpectations[] = {
    // Raw.
    {"arw", kGalleryAppId},
    {"cr2", kGalleryAppId},
    {"dng", kGalleryAppId},
    {"nef", kGalleryAppId},
    {"nrw", kGalleryAppId},
    {"orf", kGalleryAppId},
    {"raf", kGalleryAppId},
    {"rw2", kGalleryAppId},

    // Video.
    {"3gp", kVideoPlayerAppId},
    {"avi", kVideoPlayerAppId},
    {"m4v", kVideoPlayerAppId},
    {"mkv", kVideoPlayerAppId},
    {"mov", kVideoPlayerAppId},
    {"mp4", kVideoPlayerAppId},
    {"mpeg", kVideoPlayerAppId},
    {"mpeg4", kVideoPlayerAppId},
    {"mpg", kVideoPlayerAppId},
    {"mpg4", kVideoPlayerAppId},
    {"ogm", kVideoPlayerAppId},
    {"ogv", kVideoPlayerAppId},
    {"ogx", kVideoPlayerAppId},
    {"webm", kVideoPlayerAppId},

    // Audio.
    {"amr", kAudioPlayerAppId},
    {"flac", kAudioPlayerAppId},
    {"m4a", kAudioPlayerAppId},
    {"mp3", kAudioPlayerAppId},
    {"oga", kAudioPlayerAppId},
    {"ogg", kAudioPlayerAppId},
    {"wav", kAudioPlayerAppId},
};

}  // namespace

// Test file extensions correspond to mime types where expected.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTest, ExtensionToMimeMapping) {
  constexpr struct {
    const char* file_extension;
    bool has_mime = true;
  } kExpectations[] = {
      // Images.
      {"bmp"},
      {"gif"},
      {"ico"},
      {"jpg"},
      {"jpeg"},
      {"png"},
      {"webp"},

      // Raw.
      {"arw", false},
      {"cr2", false},
      {"dng", false},
      {"nef", false},
      {"nrw", false},
      {"orf", false},
      {"raf", false},
      {"rw2", false},

      // Video.
      {"3gp", false},
      {"avi", false},
      {"m4v"},
      {"mkv", false},
      {"mov", false},
      {"mp4"},
      {"mpeg"},
      {"mpeg4", false},
      {"mpg"},
      {"mpg4", false},
      {"ogm"},
      {"ogv"},
      {"ogx", false},
      {"webm"},

      // Audio.
      {"amr", false},
      {"flac"},
      {"m4a"},
      {"mp3"},
      {"oga"},
      {"ogg"},
      {"wav"},
  };

  const base::FilePath prefix = base::FilePath().AppendASCII("file");
  std::string mime_type;

  for (const auto& test : kExpectations) {
    base::FilePath path = prefix.AddExtension(test.file_extension);

    EXPECT_EQ(test.has_mime, net::GetMimeTypeFromFile(path, &mime_type))
        << test.file_extension;
  }
}

// Tests the default handlers for various file types in ChromeOS. This test
// exists to ensure the default app that launches when you open a file in the
// ChromeOS file manager does not change unexpectedly. Multiple default apps are
// allowed to register a handler for the same file type. Without that, it is not
// possible for an app to open that type even when given explicit direction via
// the chrome.fileManagerPrivate.executeTask API. The current conflict
// resolution mechanism is "sort by extension ID", which has the desired result.
// If desires change, we'll need to update ChooseAndSetDefaultTask() with some
// additional logic.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTest, DefaultHandlerChangeDetector) {
  //  With the Media App disabled, all images should be handled by Gallery.
  std::vector<Expectation> expectations = {
      // Images.
      {"bmp", kGalleryAppId},  {"gif", kGalleryAppId},  {"ico", kGalleryAppId},
      {"jpg", kGalleryAppId},  {"jpeg", kGalleryAppId}, {"png", kGalleryAppId},
      {"webp", kGalleryAppId},
  };
  expectations.insert(expectations.end(), std::begin(kUnchangedExpectations),
                      std::end(kUnchangedExpectations));

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Spot test the default handlers for selections that include multiple different
// file types. Only tests combinations of interest to the Media App.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTest, MultiSelectDefaultHandler) {
  std::vector<Expectation> expectations = {
      {"jpg/gif", kGalleryAppId},
      {"jpg/avi", kGalleryAppId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Tests the default handlers with the Media App installed.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTestWithMediaApp,
                       DefaultHandlerChangeDetector) {
  // With the Media App enabled, images should be handled by it by default (but
  // video, which it also handles should be unchanged).
  std::vector<Expectation> expectations = {
      // Images.
      {"bmp", kMediaAppId},  {"gif", kMediaAppId},  {"ico", kMediaAppId},
      {"jpg", kMediaAppId},  {"jpeg", kMediaAppId}, {"png", kMediaAppId},
      {"webp", kMediaAppId},
  };
  expectations.insert(expectations.end(), std::begin(kUnchangedExpectations),
                      std::end(kUnchangedExpectations));

  TestExpectationsAgainstDefaultTasks(expectations);
}

// Spot test the default handlers for selections that include multiple different
// file types with the Media App installed.
IN_PROC_BROWSER_TEST_F(FileTasksBrowserTestWithMediaApp,
                       MultiSelectDefaultHandler) {
  std::vector<Expectation> expectations = {
      {"jpg/gif", kMediaAppId},
      // Test video specifically since the Media App's manifest specifies it
      // handles video files.
      {"jpg/avi", kGalleryAppId},
  };

  TestExpectationsAgainstDefaultTasks(expectations);
}

}  // namespace file_tasks
}  // namespace file_manager
