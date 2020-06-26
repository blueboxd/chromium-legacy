// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage_queue.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent()
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~TestEvent() { EXPECT_TRUE(completed_.IsSignaled()) << "Not responded"; }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    completed_.Wait();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    DCHECK(!completed_.IsSignaled());
    return base::BindOnce(
        [](base::WaitableEvent* completed, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          completed->Signal();
        },
        base::Unretained(&completed_), base::Unretained(&result_));
  }

 private:
  base::WaitableEvent completed_;
  ResType result_;
};

class MockUploadClient : public StorageQueue::UploaderInterface {
 public:
  MockUploadClient() = default;

  void ProcessBlob(StatusOr<base::span<const uint8_t>> blob,
                   base::OnceCallback<void(bool)> processed_cb) override {
    if (!blob.ok()) {
      std::move(processed_cb).Run(UploadBlobFailure(blob.status()));
      return;
    }
    const base::span<const uint8_t> blob_data = blob.ValueOrDie();
    std::move(processed_cb)
        .Run(UploadBlob(
            std::string(reinterpret_cast<const char*>(blob_data.data()),
                        blob_data.size())));
  }

  void Completed(Status status) override { UploadComplete(status); }

  MOCK_METHOD(bool, UploadBlob, (base::StringPiece), (const));
  MOCK_METHOD(bool, UploadBlobFailure, (Status), (const));
  MOCK_METHOD(void, UploadComplete, (Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    explicit SetUp(MockUploadClient* client) : client_(client) {}
    ~SetUp() {
      EXPECT_CALL(*client_, UploadBlobFailure(_))
          .Times(0)
          .InSequence(client_->test_upload_sequence_);
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK())))
          .Times(1)
          .InSequence(client_->test_upload_sequence_);
    }

    SetUp& Required(base::StringPiece value) {
      EXPECT_CALL(*client_, UploadBlob(StrEq(std::string(value))))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(base::StringPiece value) {
      EXPECT_CALL(*client_, UploadBlob(StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

   private:
    MockUploadClient* const client_;
  };

 private:
  Sequence test_upload_sequence_;
};

class StorageQueueTest : public ::testing::TestWithParam<size_t> {
 protected:
  void SetUp() override { ASSERT_TRUE(location_.CreateUniqueTempDir()); }

  void CreateStorageQueueOrDie(const StorageQueue::Options& options) {
    ASSERT_FALSE(storage_queue_) << "StorageQueue already assigned";
    TestEvent<StatusOr<scoped_refptr<StorageQueue>>> e;
    StorageQueue::Create(
        options,
        base::BindRepeating(&StorageQueueTest::BuildMockUploader,
                            base::Unretained(this)),
        e.cb());
    StatusOr<scoped_refptr<StorageQueue>> storage_queue_result = e.result();
    ASSERT_OK(storage_queue_result) << "Failed to create StorageQueue, error="
                                    << storage_queue_result.status();
    storage_queue_ = std::move(storage_queue_result.ValueOrDie());
  }

  StorageQueue::Options BuildStorageQueueOptionsImmediate() const {
    return StorageQueue::Options()
        .set_directory(
            base::FilePath(location_.GetPath()).Append(FILE_PATH_LITERAL("D1")))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"))
        .set_total_size(GetParam());
  }

  StorageQueue::Options BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::TimeDelta::FromSeconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  StatusOr<std::unique_ptr<StorageQueue::UploaderInterface>>
  BuildMockUploader() {
    auto uploader = std::make_unique<MockUploadClient>();
    set_mock_uploader_expectations_.Call(uploader.get());
    return uploader;
  }

  void WriteStringOrDie(base::StringPiece data) {
    TestEvent<Status> w;
    ASSERT_TRUE(storage_queue_) << "StorageQueue not created yet";
    storage_queue_->Write(
        base::make_span(reinterpret_cast<const uint8_t*>(data.data()),
                        data.size()),
        w.cb());
    const Status write_result = w.result();
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(std::uint64_t seq_number) {
    TestEvent<Status> c;
    storage_queue_->Confirm(seq_number, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  base::ScopedTempDir location_;
  scoped_refptr<StorageQueue> storage_queue_;

  ::testing::MockFunction<void(MockUploadClient*)>
      set_mock_uploader_expectations_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

constexpr std::array<const char*, 3> blobs = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> more_blobs = {"More1111", "More222",
                                                   "More33"};

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndReopen) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull())).Times(0);
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(blobs[0]);
  WriteStringOrDie(blobs[1]);
  WriteStringOrDie(blobs[2]);

  storage_queue_.reset();

  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(blobs[0]);
  WriteStringOrDie(blobs[1]);
  WriteStringOrDie(blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[0])
            .Required(blobs[1])
            .Required(blobs[2]);
      }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(blobs[0]);
  WriteStringOrDie(blobs[1]);
  WriteStringOrDie(blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[0])
            .Required(blobs[1])
            .Required(blobs[2]);
      }));

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #0 and forward time again, removing blob #0
  ConfirmOrDie(/*seq_number=*/0);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[1])
            .Required(blobs[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #1 and forward time again, removing blob #1
  ConfirmOrDie(/*seq_number=*/1);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client).Required(blobs[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Add more records and verify that #2 and new records are returned.
  WriteStringOrDie(more_blobs[0]);
  WriteStringOrDie(more_blobs[1]);
  WriteStringOrDie(more_blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[2])
            .Required(more_blobs[0])
            .Required(more_blobs[1])
            .Required(more_blobs[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing blob #2
  ConfirmOrDie(/*seq_number=*/2);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(more_blobs[0])
            .Required(more_blobs[1])
            .Required(more_blobs[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[0])
            .Possible(blobs[1])
            .Possible(blobs[2]);
      }));
  WriteStringOrDie(blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[0])
            .Required(blobs[1])
            .Possible(blobs[2]);
      }));
  WriteStringOrDie(blobs[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[0])
            .Required(blobs[1])
            .Required(blobs[2]);
      }));
  WriteStringOrDie(blobs[2]);
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(blobs[0])
            .Possible(blobs[1])
            .Possible(blobs[2]);
      }));
  WriteStringOrDie(blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(blobs[0])
            .Possible(blobs[1])
            .Possible(blobs[2]);
      }));
  WriteStringOrDie(blobs[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(blobs[0])
            .Possible(blobs[1])
            .Required(blobs[2]);  // Not confirmed - hence |Required|
      }));
  WriteStringOrDie(blobs[2]);

  // Confirm #1, removing blobs #0 and #1
  ConfirmOrDie(/*seq_number=*/1);

  // Add more records and verify that #2 and new records are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[2])
            .Required(more_blobs[0])
            .Possible(more_blobs[1])
            .Possible(more_blobs[2]);
      }));
  WriteStringOrDie(more_blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[2])
            .Required(more_blobs[0])
            .Required(more_blobs[1])
            .Possible(more_blobs[2]);
      }));
  WriteStringOrDie(more_blobs[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(blobs[2])
            .Required(more_blobs[0])
            .Required(more_blobs[1])
            .Required(more_blobs[2]);
      }));
  WriteStringOrDie(more_blobs[2]);
}

INSTANTIATE_TEST_SUITE_P(VaryingFileSize,
                         StorageQueueTest,
                         testing::Values(128 * 1024LL * 1024LL,
                                         64 /* two records in file */,
                                         32 /* single record in file */));

// TODO(b/157943006): Additional tests:
// 1) Options object with a bad path.
// 2) Have bad prefix files in the directory.
// 3) Attempt to create file with duplicated file extension.
// 4) Other negative tests.

}  // namespace
}  // namespace reporting
