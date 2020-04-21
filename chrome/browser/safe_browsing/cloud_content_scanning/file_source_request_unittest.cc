// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_source_request.h"

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

void GetResultsForFileContents(const std::string& file_contents,
                               BinaryUploadService::Result* out_result,
                               BinaryUploadService::Request::Data* out_data) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, file_contents.data(), file_contents.size());

  FileSourceRequest request(file_path, base::DoNothing());

  bool called = false;
  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &out_result, &out_data](
          BinaryUploadService::Result result,
          const BinaryUploadService::Request::Data& data) {
        called = true;
        run_loop.Quit();
        *out_result = result;
        *out_data = data;
      }));
  run_loop.Run();

  EXPECT_TRUE(called);
}

}  // namespace

TEST(FileSourceRequestTest, InvalidFiles) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    // Non-existent files should return UNKNOWN and have no information set.
    FileSourceRequest request(temp_dir.GetPath().AppendASCII("not_a_real.doc"),
                              base::DoNothing());

    bool called = false;
    base::RunLoop run_loop;
    request.GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called](BinaryUploadService::Result result,
                             const BinaryUploadService::Request::Data& data) {
          called = true;
          run_loop.Quit();

          EXPECT_EQ(result, BinaryUploadService::Result::UNKNOWN);
          EXPECT_EQ(data.size, 0u);
          EXPECT_TRUE(data.contents.empty());
          EXPECT_TRUE(data.hash.empty());
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
  }

  {
    // Directories should not be used as paths passed to GetFileSHA256Blocking,
    // so they should return UNKNOWN and have no information set.
    FileSourceRequest request(temp_dir.GetPath(), base::DoNothing());

    bool called = false;
    base::RunLoop run_loop;
    request.GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called](BinaryUploadService::Result result,
                             const BinaryUploadService::Request::Data& data) {
          called = true;
          run_loop.Quit();

          EXPECT_EQ(result, BinaryUploadService::Result::UNKNOWN);
          EXPECT_EQ(data.size, 0u);
          EXPECT_TRUE(data.contents.empty());
          EXPECT_TRUE(data.hash.empty());
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
  }
}

TEST(FileSourceRequestTest, NormalFiles) {
  base::test::TaskEnvironment task_environment;

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;

  std::string normal_contents = "Normal file contents";
  GetResultsForFileContents(normal_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_EQ(data.contents, normal_contents);
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");

  std::string long_contents =
      std::string(BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(long_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_EQ(data.size, long_contents.size());
  EXPECT_EQ(data.contents, long_contents);
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "4F0E9C6A1A9A90F35B884D0F0E7343459C21060EEFEC6C0F2FA9DC1118DBE5BE");
}

TEST(FileSourceRequest, LargeFiles) {
  base::test::TaskEnvironment task_environment;

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;

  std::string large_file_contents(BinaryUploadService::kMaxUploadSizeBytes + 1,
                                  'a');
  GetResultsForFileContents(large_file_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_TOO_LARGE);
  EXPECT_EQ(data.size, large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43A5B4A25942");

  std::string very_large_file_contents(
      2 * BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(very_large_file_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_TOO_LARGE);
  EXPECT_EQ(data.size, very_large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (100 * 1024 * 1024), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "CEE41E98D0A6AD65CC0EC77A2BA50BF26D64DC9007F7F1C7D7DF68B8B71291A6");
}

TEST(FileSourceRequestTest, PopulatesDigest) {
  base::test::TaskEnvironment task_environment;
  std::string file_contents = "Normal file contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  FileSourceRequest request(file_path, base::DoNothing());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop](BinaryUploadService::Result result,
                  const BinaryUploadService::Request::Data& data) {
        run_loop.Quit();
      }));
  run_loop.Run();

  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(request.deep_scanning_request().digest(),
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
}

TEST(FileSourceRequestTest, PopulatesFilename) {
  base::test::TaskEnvironment task_environment;
  std::string file_contents = "contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  FileSourceRequest request(file_path, base::DoNothing());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop](BinaryUploadService::Result result,
                  const BinaryUploadService::Request::Data& data) {
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(request.deep_scanning_request().filename(), "foo.doc");
}

TEST(FileSourceRequestTest, CachesResults) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, normal_contents.data(), normal_contents.size());

  BinaryUploadService::Result async_result;
  BinaryUploadService::Request::Data async_data;

  FileSourceRequest request(file_path, base::DoNothing());

  bool called = false;
  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &async_result, &async_data](
          BinaryUploadService::Result result,
          const BinaryUploadService::Request::Data& data) {
        called = true;
        run_loop.Quit();
        async_result = result;
        async_data = data;
      }));
  run_loop.Run();

  ASSERT_TRUE(called);

  BinaryUploadService::Result sync_result;
  BinaryUploadService::Request::Data sync_data;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &sync_result, &sync_data](
          BinaryUploadService::Result result,
          const BinaryUploadService::Request::Data& data) {
        called = true;
        run_loop.Quit();
        sync_result = result;
        sync_data = data;
      }));

  EXPECT_EQ(sync_result, async_result);
  EXPECT_EQ(sync_data.contents, async_data.contents);
  EXPECT_EQ(sync_data.size, async_data.size);
  EXPECT_EQ(sync_data.hash, async_data.hash);
}

TEST(FileSourceRequestTest, Encrypted) {
  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  FileSourceRequest request(test_zip, base::DoNothing());

  bool called = false;
  base::RunLoop run_loop;
  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &result, &data](
          BinaryUploadService::Result tmp_result,
          const BinaryUploadService::Request::Data& tmp_data) {
        called = true;
        run_loop.Quit();
        result = tmp_result;
        data = tmp_data;
      }));
  run_loop.Run();

  ASSERT_TRUE(called);
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_ENCRYPTED);
  // du chrome/test/data/safe_browsing/download_protection -b
  EXPECT_EQ(data.size, 20015u);
  // sha256sum < chrome/test/data/safe_browsing/download_protection/\
  // encrypted.zip |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A");
  EXPECT_EQ(request.deep_scanning_request().digest(), data.hash);
}

TEST(FileSourceRequestTest, UnsupportedFileType) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.xyz");
  base::WriteFile(file_path, normal_contents.data(), normal_contents.size());

  FileSourceRequest request(file_path, base::DoNothing());
  request.set_request_dlp_scan(DlpDeepScanningClientRequest());

  bool called = false;
  base::RunLoop run_loop;
  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &result, &data](
          BinaryUploadService::Result tmp_result,
          const BinaryUploadService::Request::Data& tmp_data) {
        called = true;
        run_loop.Quit();
        result = tmp_result;
        data = tmp_data;
      }));
  run_loop.Run();

  ASSERT_TRUE(called);
  EXPECT_EQ(result, BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE);
  EXPECT_EQ(data.contents, normal_contents);
  EXPECT_EQ(data.size, normal_contents.size());
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_EQ(request.deep_scanning_request().digest(), data.hash);
}

}  // namespace safe_browsing
