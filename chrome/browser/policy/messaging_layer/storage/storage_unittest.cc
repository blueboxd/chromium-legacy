// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage.h"

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
using ::testing::Property;
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

class MockUploadClient : public Storage::UploaderInterface {
 public:
  MockUploadClient() = default;

  void ProcessBlob(Priority priority,
                   StatusOr<base::span<const uint8_t>> blob,
                   base::OnceCallback<void(bool)> processed_cb) override {
    if (!blob.ok()) {
      std::move(processed_cb).Run(UploadBlobFailure(priority, blob.status()));
      return;
    }
    const base::span<const uint8_t> blob_data = blob.ValueOrDie();
    std::move(processed_cb)
        .Run(UploadBlob(priority, std::string(reinterpret_cast<const char*>(
                                                  blob_data.data()),
                                              blob_data.size())));
  }

  void Completed(Priority priority, Status status) override {
    UploadComplete(priority, status);
  }

  MOCK_METHOD(bool, UploadBlob, (Priority, base::StringPiece), (const));
  MOCK_METHOD(bool, UploadBlobFailure, (Priority, Status), (const));
  MOCK_METHOD(void, UploadComplete, (Priority, Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    SetUp(Priority priority, MockUploadClient* client)
        : priority_(priority), client_(client) {}

    ~SetUp() {
      EXPECT_CALL(*client_, UploadBlobFailure(Eq(priority_), _))
          .Times(0)
          .InSequence(client_->test_upload_sequence_);
      EXPECT_CALL(*client_,
                  UploadComplete(Eq(priority_), Eq(Status::StatusOK())))
          .Times(1)
          .InSequence(client_->test_upload_sequence_);
    }

    SetUp& Required(base::StringPiece value) {
      EXPECT_CALL(*client_,
                  UploadBlob(Eq(priority_), StrEq(std::string(value))))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(base::StringPiece value) {
      EXPECT_CALL(*client_,
                  UploadBlob(Eq(priority_), StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

   private:
    Priority priority_;
    MockUploadClient* const client_;
  };

  // Helper class for setting up mock client expectations on empty queue.
  class SetEmpty {
   public:
    SetEmpty(Priority priority, MockUploadClient* client)
        : priority_(priority), client_(client) {}

    ~SetEmpty() {
      EXPECT_CALL(*client_, UploadBlob(Eq(priority_), _)).Times(0);
      EXPECT_CALL(*client_, UploadBlobFailure(Eq(priority_), _)).Times(0);
      EXPECT_CALL(*client_, UploadComplete(Eq(priority_),
                                           Property(&Status::error_code,
                                                    Eq(error::OUT_OF_RANGE))))
          .Times(1);
    }

   private:
    Priority priority_;
    MockUploadClient* const client_;
  };

 private:
  Sequence test_upload_sequence_;
};

class StorageTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(location_.CreateUniqueTempDir()); }

  void CreateStorageTestOrDie(const Storage::Options& options) {
    ASSERT_FALSE(storage_) << "StorageTest already assigned";
    TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    Storage::Create(options,
                    base::BindRepeating(&StorageTest::BuildMockUploader,
                                        base::Unretained(this)),
                    e.cb());
    StatusOr<scoped_refptr<Storage>> storage_result = e.result();
    ASSERT_OK(storage_result)
        << "Failed to create StorageTest, error=" << storage_result.status();
    storage_ = std::move(storage_result.ValueOrDie());
  }

  Storage::Options BuildStorageOptions() const {
    return Storage::Options().set_directory(
        base::FilePath(location_.GetPath()));
  }

  StatusOr<std::unique_ptr<Storage::UploaderInterface>> BuildMockUploader(
      Priority priority) {
    auto uploader = std::make_unique<MockUploadClient>();
    set_mock_uploader_expectations_.Call(priority, uploader.get());
    return uploader;
  }

  void WriteStringOrDie(Priority priority, base::StringPiece data) {
    TestEvent<Status> w;
    ASSERT_TRUE(storage_) << "Storage not created yet";
    storage_->Write(
        priority,
        base::make_span(reinterpret_cast<const uint8_t*>(data.data()),
                        data.size()),
        w.cb());
    const Status write_result = w.result();
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(Priority priority, std::uint64_t seq_number) {
    TestEvent<Status> c;
    storage_->Confirm(priority, seq_number, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  base::ScopedTempDir location_;
  scoped_refptr<Storage> storage_;

  ::testing::MockFunction<void(Priority, MockUploadClient*)>
      set_mock_uploader_expectations_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

constexpr std::array<const char*, 3> blobs = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> more_blobs = {"More1111", "More222",
                                                   "More33"};

TEST_F(StorageTest, WriteIntoNewStorageAndReopen) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, NotNull())).Times(0);
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, blobs[0]);
  WriteStringOrDie(FAST_BATCH, blobs[1]);
  WriteStringOrDie(FAST_BATCH, blobs[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
}

TEST_F(StorageTest, WriteIntoNewStorageReopenAndWriteMore) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, NotNull())).Times(0);
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, blobs[0]);
  WriteStringOrDie(FAST_BATCH, blobs[1]);
  WriteStringOrDie(FAST_BATCH, blobs[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, more_blobs[0]);
  WriteStringOrDie(FAST_BATCH, more_blobs[1]);
  WriteStringOrDie(FAST_BATCH, more_blobs[2]);
}

TEST_F(StorageTest, WriteIntoNewStorageAndUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, blobs[0]);
  WriteStringOrDie(FAST_BATCH, blobs[1]);
  WriteStringOrDie(FAST_BATCH, blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2]);
          }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(StorageTest, WriteIntoNewStorageReopenWriteMoreAndUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, blobs[0]);
  WriteStringOrDie(FAST_BATCH, blobs[1]);
  WriteStringOrDie(FAST_BATCH, blobs[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, more_blobs[0]);
  WriteStringOrDie(FAST_BATCH, more_blobs[1]);
  WriteStringOrDie(FAST_BATCH, more_blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(StorageTest, WriteIntoNewStorageAndFlush) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, blobs[0]);
  WriteStringOrDie(MANUAL_BATCH, blobs[1]);
  WriteStringOrDie(MANUAL_BATCH, blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(MANUAL_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2]);
          }));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_F(StorageTest, WriteIntoNewStorageReopenWriteMoreAndFlush) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, blobs[0]);
  WriteStringOrDie(MANUAL_BATCH, blobs[1]);
  WriteStringOrDie(MANUAL_BATCH, blobs[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, more_blobs[0]);
  WriteStringOrDie(MANUAL_BATCH, more_blobs[1]);
  WriteStringOrDie(MANUAL_BATCH, more_blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(MANUAL_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_F(StorageTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateStorageTestOrDie(BuildStorageOptions());

  WriteStringOrDie(FAST_BATCH, blobs[0]);
  WriteStringOrDie(FAST_BATCH, blobs[1]);
  WriteStringOrDie(FAST_BATCH, blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2]);
          }));

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #0 and forward time again, removing blob #0
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/0);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[1])
                .Required(blobs[2]);
          }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #1 and forward time again, removing blob #1
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/1);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[2]);
          }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Add more records and verify that #2 and new records are returned.
  WriteStringOrDie(FAST_BATCH, more_blobs[0]);
  WriteStringOrDie(FAST_BATCH, more_blobs[1]);
  WriteStringOrDie(FAST_BATCH, more_blobs[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing blob #2
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/2);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(StorageTest, WriteAndRepeatedlyImmediateUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Possible(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   blobs[0]);  // Immediately uploads and verifies.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   blobs[1]);  // Immediately uploads and verifies.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[0])
                .Required(blobs[1])
                .Required(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   blobs[2]);  // Immediately uploads and verifies.
}

TEST_F(StorageTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[0])
                .Possible(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[0])
                .Possible(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[0])
                .Possible(blobs[1])
                .Required(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[2]);

  // Confirm #1, removing blobs #0 and #1
  ConfirmOrDie(IMMEDIATE, /*seq_number=*/1);

  // Add more records and verify that #2 and new records are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Possible(more_blobs[1])
                .Possible(more_blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Possible(more_blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_blobs[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(blobs[2])
                .Required(more_blobs[0])
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_blobs[2]);
}

TEST_F(StorageTest, WriteAndRepeatedlyUploadMultipleQueues) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[0])
                .Possible(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[0]);
  WriteStringOrDie(SLOW_BATCH, more_blobs[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[0])
                .Possible(blobs[1])
                .Possible(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[1]);
  WriteStringOrDie(SLOW_BATCH, more_blobs[1]);

  // Set uploader expectations for SLOW_BATCH.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillRepeatedly(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetEmpty(priority, mock_upload_client);
          }));
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(SLOW_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(more_blobs[0])
                .Required(more_blobs[1]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));

  // Confirm #0 SLOW_BATCH, removing blobs #0
  ConfirmOrDie(SLOW_BATCH, /*seq_number=*/0);

  // Confirm #1 IMMEDIATE, removing blobs #0 and #1
  ConfirmOrDie(IMMEDIATE, /*seq_number=*/1);

  // Add more data
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(blobs[1])
                .Required(blobs[2]);
          }));
  WriteStringOrDie(IMMEDIATE, blobs[2]);
  WriteStringOrDie(SLOW_BATCH, more_blobs[2]);

  // Set uploader expectations for SLOW_BATCH.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillRepeatedly(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetEmpty(priority, mock_upload_client);
          }));
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(SLOW_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(SLOW_BATCH, mock_upload_client)
                .Required(more_blobs[1])
                .Required(more_blobs[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
}

}  // namespace
}  // namespace reporting
