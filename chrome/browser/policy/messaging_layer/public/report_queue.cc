// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "crypto/sha2.h"

namespace reporting {

std::unique_ptr<ReportQueue> ReportQueue::Create(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModule> storage,
    scoped_refptr<EncryptionModule> encryption) {
  return base::WrapUnique<ReportQueue>(
      new ReportQueue(std::move(config), storage, encryption));
}

ReportQueue::~ReportQueue() = default;

ReportQueue::ReportQueue(std::unique_ptr<ReportQueueConfiguration> config,
                         scoped_refptr<StorageModule> storage,
                         scoped_refptr<EncryptionModule> encryption)
    : config_(std::move(config)),
      storage_(storage),
      encryption_(encryption),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Status ReportQueue::Enqueue(base::StringPiece record,
                            EnqueueCallback callback) {
  return AddRecord(record, std::move(callback));
}

Status ReportQueue::Enqueue(const base::Value& record,
                            EnqueueCallback callback) {
  std::string json_record;
  if (!base::JSONWriter::Write(record, &json_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Provided record was not convertable to a std::string");
  }
  return AddRecord(json_record, std::move(callback));
}

Status ReportQueue::Enqueue(google::protobuf::MessageLite* record,
                            EnqueueCallback callback) {
  std::string protobuf_record;
  if (!record->SerializeToString(&protobuf_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Unabled to serialize record to string. Most likely due to "
                  "unset required fields.");
  }
  return AddRecord(protobuf_record, std::move(callback));
}

Status ReportQueue::AddRecord(base::StringPiece record,
                              EnqueueCallback callback) {
  RETURN_IF_ERROR(config_->CheckPolicy());
  if (!sequenced_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ReportQueue::SendRecordToStorage,
                                    base::Unretained(this), std::string(record),
                                    std::move(callback)))) {
    return Status(error::INTERNAL, "Failed to post the record for processing.");
  }
  return Status::StatusOK();
}

void ReportQueue::SendRecordToStorage(std::string record,
                                      EnqueueCallback callback) {
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(WrappedRecord wrapped_record, callback,
                                     WrapRecord(record));

  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(EncryptedRecord encrypted_record, callback,
                                     EncryptRecord(wrapped_record));

  storage_->AddRecord(encrypted_record, config_->priority(),
                      std::move(callback));
}

StatusOr<WrappedRecord> ReportQueue::WrapRecord(base::StringPiece record_data) {
  WrappedRecord wrapped_record;

  Record* record = wrapped_record.mutable_record();
  record->set_data(std::string(record_data));
  record->set_destination(config_->destination());
  record->set_dm_token(config_->dm_token().value());

  std::string record_digest;
  record->SerializeToString(&record_digest);
  wrapped_record.set_record_digest(crypto::SHA256HashString(record_digest));

  ASSIGN_OR_RETURN(*wrapped_record.mutable_last_record_digest(),
                   GetLastRecordDigest());
  return wrapped_record;
}

StatusOr<std::string> ReportQueue::GetLastRecordDigest() {
  // TODO(b/153659559) Getting the actual last record digest will come later.
  // For now we just set to a string.
  return "LastRecordDigest";
}

StatusOr<EncryptedRecord> ReportQueue::EncryptRecord(
    WrappedRecord wrapped_record) {
  std::string serialized_wrapped_record;
  wrapped_record.SerializeToString(&serialized_wrapped_record);

  ASSIGN_OR_RETURN(std::string encrypted_string_record,
                   encryption_->EncryptRecord(serialized_wrapped_record));

  EncryptedRecord encrypted_record;
  encrypted_record.set_encrypted_wrapped_record(encrypted_string_record);

  auto* sequencing_information =
      encrypted_record.mutable_sequencing_information();
  sequencing_information->set_priority(config_->priority());

  return encrypted_record;
}

}  // namespace reporting
