// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/common/chrome_paths.h"

namespace reporting {

namespace {

const base::FilePath::CharType kReportingDirectory[] =
    FILE_PATH_LITERAL("reporting");

}  // namespace

class ReportingClient::UploadClient : public Storage::UploaderInterface {
 public:
  static StatusOr<std::unique_ptr<Storage::UploaderInterface>> Build(
      Priority priority) {
    // Cannot use make_unique, since constructor is private.
    return base::WrapUnique(new UploadClient());
  }

  void ProcessBlob(Priority priority,
                   StatusOr<base::span<const uint8_t>> blob,
                   base::OnceCallback<void(bool)> processed_cb) override {
    std::move(processed_cb).Run(false);  // Do not proceed.
  }

  void Completed(Priority priority, Status status) override {
    LOG(ERROR) << "Not implemented yet, status=" << status;
  }

 private:
  UploadClient() = default;
};

ReportingClient::ReportingClient(scoped_refptr<StorageModule> storage)
    : storage_(std::move(storage)),
      encryption_(base::MakeRefCounted<EncryptionModule>()) {}

ReportingClient::~ReportingClient() = default;

StatusOr<std::unique_ptr<ReportQueue>> ReportingClient::CreateReportQueue(
    std::unique_ptr<ReportQueueConfiguration> config) {
  ASSIGN_OR_RETURN(ReportingClient* instance, GetInstance());
  return ReportQueue::Create(std::move(config), instance->storage_,
                             instance->encryption_);
}

StatusOr<ReportingClient*> ReportingClient::GetInstance() {
  static base::NoDestructor<StatusOr<std::unique_ptr<ReportingClient>>>
      instance(Create());
  if (!instance->ok()) {
    return instance->status();
  }
  return instance->ValueOrDie().get();
}

// TODO(chromium:1078512) As part of completing the EncryptionModule,
// this create function will need to be updated to check for
// successful creation of the EncryptionModule too.
StatusOr<std::unique_ptr<ReportingClient>> ReportingClient::Create() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return Status(error::FAILED_PRECONDITION, "Could not retrieve base path");
  }
  base::FilePath reporting_path = user_data_dir.Append(kReportingDirectory);
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  StatusOr<scoped_refptr<StorageModule>> storage_result;
  StorageModule::Create(
      Storage::Options().set_directory(reporting_path),
      base::BindRepeating(&ReportingClient::UploadClient::Build),
      base::BindOnce(
          [](StatusOr<scoped_refptr<StorageModule>>* result,
             base::WaitableEvent* done,
             StatusOr<scoped_refptr<StorageModule>> storage) {
            *result = std::move(storage);
            done->Signal();
          },
          base::Unretained(&storage_result), base::Unretained(&done)));
  done.Wait();
  RETURN_IF_ERROR(storage_result.status());
  auto client = base::WrapUnique<ReportingClient>(
      new ReportingClient(std::move(storage_result.ValueOrDie())));
  return client;
}

}  // namespace reporting
