// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"

#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Return;
using ::testing::ReturnRef;

class DownloadItemRequestTest : public ::testing::TestWithParam<bool> {
 public:
  DownloadItemRequestTest()
      : item_(),
        request_(&item_, /*read_immediately=*/false, base::DoNothing()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    download_path_ = temp_dir_.GetPath().AppendASCII("download_location");
    download_temporary_path_ =
        temp_dir_.GetPath().AppendASCII("temporary_location");

    base::File file(download_path_,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());

    download_contents_ = large_contents() ? std::string(51 * 1024 * 1024, 'a')
                                          : "download contents";
    file.Write(0, download_contents_.c_str(), download_contents_.size());
    file.Close();

    EXPECT_CALL(item_, GetTotalBytes())
        .WillRepeatedly(Return(download_contents_.size()));
    EXPECT_CALL(item_, GetTargetFilePath())
        .WillRepeatedly(ReturnRef(download_path_));
    EXPECT_CALL(item_, GetFullPath()).WillRepeatedly(ReturnRef(download_path_));
  }

  bool large_contents() const { return GetParam(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  download::MockDownloadItem item_;
  DownloadItemRequest request_;
  base::ScopedTempDir temp_dir_;
  base::FilePath download_path_;
  base::FilePath download_temporary_path_;
  std::string download_contents_;
};

TEST_P(DownloadItemRequestTest, GetsContentsWaitsUntilRename) {
  ON_CALL(item_, GetFullPath())
      .WillByDefault(ReturnRef(download_temporary_path_));

  std::string download_contents;
  request_.GetRequestData(base::BindOnce(
      [](std::string* target_contents, BinaryUploadService::Result result,
         const BinaryUploadService::Request::Data& data) {
        *target_contents = data.contents;
      },
      &download_contents));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(download_contents, "");

  ON_CALL(item_, GetFullPath()).WillByDefault(ReturnRef(download_path_));
  item_.NotifyObserversDownloadUpdated();

  content::RunAllTasksUntilIdle();

  // The contents should not be read if they are too large.
  if (large_contents())
    EXPECT_EQ(download_contents, "");
  else
    EXPECT_EQ(download_contents, "download contents");
}

INSTANTIATE_TEST_SUITE_P(DownloadItemRequestTest,
                         DownloadItemRequestTest,
                         testing::Bool());

}  // namespace safe_browsing
