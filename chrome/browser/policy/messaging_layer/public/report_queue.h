// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/record.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record_constants.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// TODO(b/153659559) Temporary StorageModule until the real one is ready.
class StorageModule : public base::RefCounted<StorageModule> {
 public:
  StorageModule() = default;

  StorageModule(const StorageModule& other) = delete;
  StorageModule& operator=(const StorageModule& other) = delete;

  // AddRecord will add |record| to the |StorageModule| according to the
  // provided |priority|. On completion, |callback| will be called.
  virtual void AddRecord(reporting_messaging_layer::EncryptedRecord record,
                         reporting_messaging_layer::Priority priority,
                         base::OnceCallback<void(Status)> callback) = 0;

 protected:
  virtual ~StorageModule() = default;

 private:
  friend base::RefCounted<StorageModule>;
};

// TODO(b/153659559) Temporary EncryptionModule until the real one is ready.
class EncryptionModule : public base::RefCounted<EncryptionModule> {
 public:
  EncryptionModule() = default;

  EncryptionModule(const EncryptionModule& other) = delete;
  EncryptionModule& operator=(const EncryptionModule& other) = delete;

  // EncryptRecord will attempt to encrypt the provided |record|. On success the
  // return value will contain the encrypted string.
  virtual StatusOr<std::string> EncryptRecord(base::StringPiece record) = 0;

 protected:
  virtual ~EncryptionModule() = default;

 private:
  friend base::RefCounted<EncryptionModule>;
};

// A |ReportQueue| is configured with a |ReportQueueConfiguration|.  A
// |ReportQueue| allows a user to |Enqueue| a message for delivery to a handler
// specified by the |Destination| held by the provided
// |ReportQueueConfiguration|. |ReportQueue| handles scheduling encryption,
// storage, and delivery.
//
// TODO(b/156099331): |ReportQueue|s should not be created or managed directly,
// they should instead be created with the |ReportingClientLibrary| which does
// not exist at this time.
//
// Example Usage:
// Status SendMessage(google::protobuf::ImportantMessage important_message,
//                    base::OnceCallback<void(Status)> callback) {
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueueConfiguration> config,
//                  ReportQueueConfiguration::Create(...));
//   ASSIGN_OR_RETURN(std::unique_ptr<ReportQueue> report_queue,
//                  ReportingClientLibrary::CreateReportQueue(config));
//   return report_queue->Enqueue(important_message, callback);
// }
//
// Enqueue can also be used with a |base::Value| or |std::string|.
class ReportQueue {
 public:
  // An EnqueueCallbacks are called on the completion of any |Enqueue| call.
  using EnqueueCallback = base::OnceCallback<void(Status)>;

  // Factory
  static std::unique_ptr<ReportQueue> Create(
      std::unique_ptr<ReportQueueConfiguration> config,
      scoped_refptr<StorageModule> storage,
      scoped_refptr<EncryptionModule> encryption);

  ~ReportQueue();
  ReportQueue(const ReportQueue& other) = delete;
  ReportQueue& operator=(const ReportQueue& other) = delete;

  // Enqueue asynchronously encrypts, stores, and delivers a record. Enqueue
  // will return an OK status if the task is successfully scheduled. The
  // |callback| will be called on any errors during encryption or storage. If
  // storage is successful |callback| will be called with an OK status.
  //
  // The current destinations have the following data requirements:
  // (destination : requirement)
  // UPLOAD_EVENTS : UploadEventsRequest
  //
  // |record| will be sent as a string with no conversion.
  Status Enqueue(base::StringPiece record, EnqueueCallback callback);

  // |record| will be converted to a JSON string with base::JsonWriter::Write.
  Status Enqueue(const base::Value& record, EnqueueCallback callback);

  // |record| will be converted to a string with SerializeToString(). The
  // handler is responsible for converting the record back to a proto with a
  // ParseFromString() call.
  Status Enqueue(google::protobuf::MessageLite* record,
                 EnqueueCallback callback);

 private:
  ReportQueue(std::unique_ptr<ReportQueueConfiguration> config,
              scoped_refptr<StorageModule> storage,
              scoped_refptr<EncryptionModule> encryption);

  Status AddRecord(base::StringPiece record, EnqueueCallback callback);
  void SendRecordToStorage(std::string record, EnqueueCallback callback);

  StatusOr<reporting_messaging_layer::WrappedRecord> WrapRecord(
      base::StringPiece record_data);
  StatusOr<std::string> GetLastRecordDigest();
  StatusOr<reporting_messaging_layer::EncryptedRecord> EncryptRecord(
      reporting_messaging_layer::WrappedRecord wrapped_record);

  std::unique_ptr<ReportQueueConfiguration> config_;
  scoped_refptr<StorageModule> storage_;
  scoped_refptr<EncryptionModule> encryption_;
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_H_
