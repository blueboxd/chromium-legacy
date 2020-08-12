// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
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
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "chrome/common/chrome_paths.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif

namespace reporting {

namespace {

// policy::CloudPolicyClient is needed by the UploadClient, but is built in two
// different ways for ChromeOS and non-ChromeOS browsers.
#if defined(OS_CHROMEOS)
std::unique_ptr<policy::CloudPolicyClient> BuildCloudPolicyClient() {
  policy::DeviceManagementService* const device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();

  scoped_refptr<network::SharedURLLoaderFactory>
      signin_profile_url_loader_factory =
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory();

  auto* user_manager_ptr = g_browser_process->platform_part()->user_manager();
  auto* primary_user = user_manager_ptr->GetPrimaryUser();

  auto dm_token_getter = chromeos::GetDeviceDMTokenForUserPolicyGetter(
      primary_user->GetAccountId());

  auto client = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, signin_profile_url_loader_factory,
      dm_token_getter);

  policy::CloudPolicyClient::RegistrationParameters registration(
      enterprise_management::DeviceRegisterRequest::USER,
      enterprise_management::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);

  // Register the client with the device management service.
  client->Register(registration,
                   /*client_id=*/std::string(),
                   /*oauth_token=*/"oauth_token_unused");
  return client;
}
#else
std::unique_ptr<policy::CloudPolicyClient> BuildCloudPolicyClient() {
  policy::DeviceManagementService* const device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();

  scoped_refptr<network::SharedURLLoaderFactory>
      signin_profile_url_loader_factory =
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory();

  auto client = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, signin_profile_url_loader_factory,
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  policy::DMToken browser_dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  client->SetupRegistration(browser_dm_token.value(), client_id,
                            std::vector<std::string>());
  return client;
}
#endif

const base::FilePath::CharType kReportingDirectory[] =
    FILE_PATH_LITERAL("reporting");

}  // namespace

using Uploader = ReportingClient::Uploader;

Uploader::Uploader(UploadCallback upload_callback)
    : upload_callback_(std::move(upload_callback)),
      completed_(false),
      encrypted_records_(std::make_unique<std::vector<EncryptedRecord>>()) {}

Uploader::~Uploader() = default;

StatusOr<std::unique_ptr<Uploader>> Uploader::Create(
    UploadCallback upload_callback) {
  auto uploader = base::WrapUnique(new Uploader(std::move(upload_callback)));
  return uploader;
}

void Uploader::ProcessBlob(Priority priority,
                           StatusOr<base::span<const uint8_t>> data,
                           base::OnceCallback<void(bool)> processed_cb) {
  if (completed_ || !data.ok()) {
    std::move(processed_cb).Run(false);
    return;
  }

  class ProcessBlobContext : public TaskRunnerContext<bool> {
   public:
    ProcessBlobContext(
        base::span<const uint8_t> data,
        std::vector<EncryptedRecord>* records,
        base::OnceCallback<void(bool)> processed_callback,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
        : TaskRunnerContext<bool>(std::move(processed_callback),
                                  sequenced_task_runner),
          records_(records),
          data_(data.begin(), data.end()) {}

   private:
    ~ProcessBlobContext() override = default;

    void OnStart() override {
      if (data_.empty()) {
        Complete(true);
        return;
      }
      ProcessBlob();
    }

    void ProcessBlob() {
      EncryptedRecord record;
      if (!record.ParseFromArray(data_.data(), data_.size())) {
        Complete(false);
        return;
      }
      records_->push_back(record);
      Complete(true);
    }

    void Complete(bool success) {
      if (!success) {
        LOG(ERROR) << "Unable to process blob";
      }
      Response(success);
    }

    std::vector<EncryptedRecord>* const records_;
    const std::vector<const uint8_t> data_;
  };

  Start<ProcessBlobContext>(data.ValueOrDie(), encrypted_records_.get(),
                            std::move(processed_cb), sequenced_task_runner_);
}

void Uploader::Completed(Priority priority, Status final_status) {
  if (!final_status.ok()) {
    // No work to do - something went wrong with storage and it no longer wants
    // to upload the records. Let the records die with |this|.
    return;
  }

  if (completed_) {
    // RunUpload has already been invoked. Return.
    return;
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Uploader::RunUpload, base::Unretained(this)));
}

void Uploader::RunUpload() {
  if (completed_) {
    // RunUpload has already been invoked. Return.
    return;
  }
  completed_ = true;

  Status upload_status =
      std::move(upload_callback_).Run(std::move(encrypted_records_));
  if (!upload_status.ok()) {
    LOG(ERROR) << "Unable to upload records: " << upload_status;
  }
}

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
      base::BindRepeating(&ReportingClient::BuildUploader),
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

// static
StatusOr<std::unique_ptr<Storage::UploaderInterface>>
ReportingClient::BuildUploader(Priority priority) {
  ASSIGN_OR_RETURN(ReportingClient * instance, GetInstance());
  if (instance->upload_client_ == nullptr) {
    ASSIGN_OR_RETURN(
        instance->upload_client_,
        UploadClient::Create(BuildCloudPolicyClient(),
                             base::BindRepeating(&StorageModule::ReportSuccess,
                                                 instance->storage_)));
  }
  return Uploader::Create(
      base::BindOnce(&UploadClient::EnqueueUpload,
                     base::Unretained(instance->upload_client_.get())));
}

}  // namespace reporting
