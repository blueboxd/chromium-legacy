// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/native_file_system/file_system_chooser_test_helpers.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

// This browser test implements end-to-end tests for
// NativeFileSystemFileWriterImpl.
class NativeFileSystemFileWriterBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);

    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  std::pair<base::FilePath, base::FilePath> CreateTestFilesAndEntry(
      const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_file;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file));
    EXPECT_EQ(int{contents.size()},
              base::WriteFile(test_file, contents.data(), contents.size()));

    ui::SelectFileDialog::SetFactory(
        new FakeSelectFileDialogFactory({test_file}));
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
    EXPECT_EQ(
        test_file.BaseName().AsUTF8Unsafe(),
        EvalJs(
            shell(),
            "(async () => {"
            "  let e = await self.chooseFileSystemEntries({type: 'openFile'});"
            "  self.entry = e;"
            "  self.writers = [];"
            "  return e.name; })()"));

    const base::FilePath swap_file =
        base::FilePath(test_file).AddExtensionASCII(".crswap");
    return std::make_pair(test_file, swap_file);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       ContentsWrittenToSwapFileFirst) {
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry("");
  const std::string file_contents = "file contents to write";

  EXPECT_EQ(0,
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
                             "  self.writer = w;"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));
  {
    // Destination file should be empty, contents written in the swap file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ("", read_contents);
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ(file_contents, swap_contents);
  }

  // Contents now in destination file.
  EXPECT_EQ(int{file_contents.size()},
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);

    EXPECT_FALSE(base::PathExists(swap_file));
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       KeepExistingDataHasPreviousContent) {
  const std::string initial_contents = "fooks";
  const std::string expected_contents = "barks";
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(true, EvalJs(shell(),
                         "(async () => {"
                         "  try {"
                         "    const w = await self.entry.createWriter({"
                         "      keepExistingData: true });"
                         "    self.writer = w;"
                         "    return true;"
                         "  } catch (e) { return false; }"
                         "})()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(swap_file));
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ(initial_contents, swap_contents);
  }

  EXPECT_EQ(int{expected_contents.size()},
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(0, new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       CreateWriterNoKeepExistingWithEmptyFile) {
  const std::string initial_contents = "very long string";
  const std::string expected_contents = "bar";
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(true, EvalJs(shell(),
                         "(async () => {"
                         "  try {"
                         "    const w = await self.entry.createWriter({"
                         "      keepExistingData: false });"
                         "    self.writer = w;"
                         "    return true;"
                         "  } catch (e) { return false; }"
                         "})()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(swap_file));
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ("", swap_contents);
  }

  EXPECT_EQ(int{expected_contents.size()},
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(0, new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFile) {
  base::FilePath test_file, base_swap_file;
  std::tie(test_file, base_swap_file) = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(true, EvalJs(shell(),
                           "(async () => {"
                           "  try {"
                           "    const w = await self.entry.createWriter();"
                           "    self.writers.push(w);"
                           "    return true;"
                           "  } catch (e) { return false; }"
                           "})()"));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (int index = 0; index < num_writers; index++) {
      base::FilePath swap_file = base_swap_file;
      if (index != 0) {
        swap_file = base::FilePath(test_file).AddExtensionASCII(
            base::StringPrintf(".%d.crswap", index));
      }
      EXPECT_TRUE(base::PathExists(swap_file));
    }
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFileRacy) {
  base::FilePath test_file, base_swap_file;
  std::tie(test_file, base_swap_file) = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(
        true,
        EvalJs(shell(),
               JsReplace("(async () => {"
                         "  try {"
                         "    for(let i = 0; i < $1; i++ ) {"
                         "      self.writers.push(self.entry.createWriter());"
                         "    }"
                         "    await Promise.all(self.writers);"
                         "    return true;"
                         "  } catch (e) { return false; }"
                         "})()",
                         num_writers)));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (int index = 0; index < num_writers; index++) {
      base::FilePath swap_file = base_swap_file;
      if (index != 0) {
        swap_file = base::FilePath(test_file).AddExtensionASCII(
            base::StringPrintf(".%d.crswap", index));
      }
      EXPECT_TRUE(base::PathExists(swap_file));
    }
  }
}

}  // namespace content
