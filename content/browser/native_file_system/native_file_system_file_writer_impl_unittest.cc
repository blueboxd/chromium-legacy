// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"

#include <limits>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using storage::FileSystemURL;

namespace content {

class NativeFileSystemFileWriterImplTest : public testing::Test {
 public:
  NativeFileSystemFileWriterImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    test_url_ = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe("test"));

    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateFile(
                                       file_system_context_.get(), test_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(), nullptr);
    blob_context_ = chrome_blob_context_->context();

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr);

    handle_ = std::make_unique<NativeFileSystemFileWriterImpl>(
        manager_.get(),
        NativeFileSystemManagerImpl::BindingContext(
            test_url_.origin(), /*process_id=*/1,
            /*frame_id=*/MSG_ROUTING_NONE),
        test_url_,
        NativeFileSystemManagerImpl::SharedHandleState(
            permission_grant_, permission_grant_, /*file_system=*/{}));
  }

  blink::mojom::BlobPtr CreateBlob(const std::string& contents) {
    auto builder =
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
    builder->AppendData(contents);
    auto handle = blob_context_->AddFinishedBlob(std::move(builder));
    blink::mojom::BlobPtr result;
    storage::BlobImpl::Create(std::move(handle), MakeRequest(&result));
    return result;
  }

  mojo::ScopedDataPipeConsumerHandle CreateStream(const std::string& contents) {
    // Test with a relatively low capacity pipe to make sure it isn't all
    // written/read in one go.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 16;
    mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
    CHECK(producer_handle.is_valid());
    auto producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    auto* producer_raw = producer.get();
    producer_raw->Write(
        std::make_unique<mojo::StringDataSource>(
            contents, mojo::StringDataSource::AsyncWritingMode::
                          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(
            base::DoNothing::Once<std::unique_ptr<mojo::DataPipeProducer>,
                                  MojoResult>(),
            std::move(producer)));
    return consumer_handle;
  }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = callback.WaitForResult();
      EXPECT_GE(rv, 0);
      if (rv < 0)
        return "(read failure)";
      if (rv == 0)
        return result;
      result.append(buf->data(), rv);
    }
  }

  base::File::Error WriteBlobSync(uint64_t position,
                                  blink::mojom::BlobPtr blob,
                                  uint64_t* bytes_written_out) {
    base::RunLoop loop;
    base::File::Error result_out;
    handle_->Write(position, std::move(blob),
                   base::BindLambdaForTesting(
                       [&](blink::mojom::NativeFileSystemErrorPtr result,
                           uint64_t bytes_written) {
                         result_out = result->error_code;
                         *bytes_written_out = bytes_written;
                         loop.Quit();
                       }));
    loop.Run();
    return result_out;
  }

  base::File::Error WriteStreamSync(
      uint64_t position,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      uint64_t* bytes_written_out) {
    base::RunLoop loop;
    base::File::Error result_out;
    handle_->WriteStream(position, std::move(data_pipe),
                         base::BindLambdaForTesting(
                             [&](blink::mojom::NativeFileSystemErrorPtr result,
                                 uint64_t bytes_written) {
                               result_out = result->error_code;
                               *bytes_written_out = bytes_written;
                               loop.Quit();
                             }));
    loop.Run();
    return result_out;
  }

  base::File::Error TruncateSync(uint64_t length) {
    base::RunLoop loop;
    base::File::Error result_out;
    handle_->Truncate(length,
                      base::BindLambdaForTesting(
                          [&](blink::mojom::NativeFileSystemErrorPtr result) {
                            result_out = result->error_code;
                            loop.Quit();
                          }));
    loop.Run();
    return result_out;
  }

  base::File::Error CloseSync() {
    base::RunLoop loop;
    base::File::Error result_out;
    handle_->Close(base::BindLambdaForTesting(
        [&](blink::mojom::NativeFileSystemErrorPtr result) {
          result_out = result->error_code;
          loop.Quit();
        }));
    loop.Run();
    return result_out;
  }

  virtual bool WriteUsingBlobs() { return true; }

  base::File::Error WriteSync(uint64_t position,
                              const std::string& contents,
                              uint64_t* bytes_written_out) {
    if (WriteUsingBlobs())
      return WriteBlobSync(position, CreateBlob(contents), bytes_written_out);
    return WriteStreamSync(position, CreateStream(contents), bytes_written_out);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestBrowserThreadBundle scoped_task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  storage::BlobStorageContext* blob_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;

  FileSystemURL test_url_;

  scoped_refptr<FixedNativeFileSystemPermissionGrant> permission_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::GRANTED);
  std::unique_ptr<NativeFileSystemFileWriterImpl> handle_;
};

class NativeFileSystemFileWriterImplWriteTest
    : public NativeFileSystemFileWriterImplTest,
      public testing::WithParamInterface<bool> {
 public:
  bool WriteUsingBlobs() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(NativeFileSystemFileWriterImplTest,
                         NativeFileSystemFileWriterImplWriteTest,
                         ::testing::Bool());

TEST_F(NativeFileSystemFileWriterImplTest, WriteInvalidBlob) {
  blink::mojom::BlobPtr blob;
  MakeRequest(&blob);

  uint64_t bytes_written;
  base::File::Error result = WriteBlobSync(0, std::move(blob), &bytes_written);
  EXPECT_EQ(result, base::File::FILE_ERROR_FAILED);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteValidEmptyString) {
  uint64_t bytes_written;
  base::File::Error result = WriteSync(0, "", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteValidNonEmpty) {
  std::string test_data("abcdefghijklmnopqrstuvwxyz");
  uint64_t bytes_written;
  base::File::Error result = WriteSync(0, test_data, &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, test_data.size());

  EXPECT_EQ(test_data, ReadFile(test_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteWithOffsetInFile) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 10u);

  result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 3u);

  EXPECT_EQ("1234abc890", ReadFile(test_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteWithOffsetPastFile) {
  uint64_t bytes_written;
  base::File::Error result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_ERROR_FAILED);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, CloseOK) {
  uint64_t bytes_written;
  base::File::Error result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 3u);
  result = CloseSync();
  EXPECT_EQ(result, base::File::FILE_OK);

  EXPECT_EQ("abc", ReadFile(test_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, TruncateShrink) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 10u);

  result = TruncateSync(5);
  EXPECT_EQ(result, base::File::FILE_OK);

  EXPECT_EQ("12345", ReadFile(test_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, TruncateGrow) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 3u);

  result = TruncateSync(5);
  EXPECT_EQ(result, base::File::FILE_OK);

  EXPECT_EQ(std::string("abc\0\0", 5), ReadFile(test_url_));
}

// TODO(mek): More tests, particularly for error conditions.

}  // namespace content
