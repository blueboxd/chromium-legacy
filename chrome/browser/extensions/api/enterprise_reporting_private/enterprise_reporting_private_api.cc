// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace extensions {
namespace {

void LogReportError(const std::string& reason) {
  VLOG(1) << "Enterprise report is not uploaded: " << reason;
}

}  // namespace

namespace enterprise_reporting {

const char kInvalidInputErrorMessage[] = "The report is not valid.";
const char kUploadFailed[] = "Failed to upload the report.";
const char kDeviceNotEnrolled[] = "This device has not been enrolled yet.";
const char kDeviceIdNotFound[] = "Failed to retrieve the device id.";
const char kEndpointVerificationRetrievalFailed[] =
    "Failed to retrieve the endpoint verification data.";
const char kEndpointVerificationStoreFailed[] =
    "Failed to store the endpoint verification data.";
// const char kEndpointVerificationSecretRetrievalFailed[] = "Failed to retrieve
// the endpoint verification secret.";

}  // namespace enterprise_reporting

// UploadDesktopReport

EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction()
    : EnterpriseReportingPrivateUploadChromeDesktopReportFunction(
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory()) {}

EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  // Initial the DeviceManagementService if it exist and hasn't been initialized
  if (device_management_service)
    device_management_service->ScheduleInitialization(0);

  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, std::move(url_loader_factory),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  dm_token_ = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  client_id_ = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    ~EnterpriseReportingPrivateUploadChromeDesktopReportFunction() {}

// static
EnterpriseReportingPrivateUploadChromeDesktopReportFunction*
EnterpriseReportingPrivateUploadChromeDesktopReportFunction::CreateForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return new EnterpriseReportingPrivateUploadChromeDesktopReportFunction(
      url_loader_factory);
}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateUploadChromeDesktopReportFunction::Run() {
  VLOG(1) << "Uploading enterprise report";

  if (!dm_token_.is_valid() || client_id_.empty()) {
    LogReportError("Device is not enrolled.");
    return RespondNow(Error(enterprise_reporting::kDeviceNotEnrolled));
  }
  std::unique_ptr<
      api::enterprise_reporting_private::UploadChromeDesktopReport::Params>
      params(api::enterprise_reporting_private::UploadChromeDesktopReport::
                 Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(
          params->report.additional_properties,
          Profile::FromBrowserContext(browser_context()));
  if (!request) {
    LogReportError("The input from extension is not valid.");
    return RespondNow(Error(enterprise_reporting::kInvalidInputErrorMessage));
  }

  if (!cloud_policy_client_->is_registered())
    cloud_policy_client_->SetupRegistration(dm_token_.value(), client_id_,
                                            std::vector<std::string>());

  cloud_policy_client_->UploadChromeDesktopReport(
      std::move(request),
      base::BindOnce(
          &EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
              OnReportUploaded,
          this));
  return RespondLater();
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    SetCloudPolicyClientForTesting(
        std::unique_ptr<policy::CloudPolicyClient> client) {
  cloud_policy_client_ = std::move(client);
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    SetRegistrationInfoForTesting(const policy::DMToken& dm_token,
                                  const std::string& client_id) {
  dm_token_ = dm_token;
  client_id_ = client_id;
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    OnReportUploaded(bool status) {
  // Schedule to delete |cloud_policy_client_| later, as we'll be deleted right
  // after calling Respond but |cloud_policy_client_| is not done yet.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, cloud_policy_client_.release());
  if (status) {
    VLOG(1) << "The enterprise report has been uploaded.";
    Respond(NoArguments());
  } else {
    LogReportError("Server error.");
    Respond(Error(enterprise_reporting::kUploadFailed));
  }
}

// GetDeviceId

EnterpriseReportingPrivateGetDeviceIdFunction::
    EnterpriseReportingPrivateGetDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceIdFunction::Run() {
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id.empty())
    return RespondNow(Error(enterprise_reporting::kDeviceIdNotFound));
  return RespondNow(OneArgument(std::make_unique<base::Value>(client_id)));
}

EnterpriseReportingPrivateGetDeviceIdFunction::
    ~EnterpriseReportingPrivateGetDeviceIdFunction() = default;

// getPersistentSecret

EnterpriseReportingPrivateGetPersistentSecretFunction::
    EnterpriseReportingPrivateGetPersistentSecretFunction() = default;
EnterpriseReportingPrivateGetPersistentSecretFunction::
    ~EnterpriseReportingPrivateGetPersistentSecretFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetPersistentSecretFunction::Run() {
  // TODO(pastarmovj): Consider keying the secret retrieval by extension id.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceSecret,
          base::BindOnce(
              &EnterpriseReportingPrivateGetPersistentSecretFunction::
                  OnDataRetrieved,
              this)));
  return RespondLater();
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::OnDataRetrieved(
    const std::string& data,
    bool status) {
  if (status) {
    VLOG(1) << "The Endpoint Verification secret was retrieved.";
    Respond(OneArgument(std::make_unique<base::Value>(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
  } else {
    LogReportError("Endpoint Verification secret retrieval error.");
    Respond(Error(enterprise_reporting::kEndpointVerificationRetrievalFailed));
  }
}

// getDeviceData

EnterpriseReportingPrivateGetDeviceDataFunction::
    EnterpriseReportingPrivateGetDeviceDataFunction() = default;
EnterpriseReportingPrivateGetDeviceDataFunction::
    ~EnterpriseReportingPrivateGetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::GetDeviceData::Params>
      params(api::enterprise_reporting_private::GetDeviceData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceData, params->id,
          base::BindOnce(
              &EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved,
              this)));
  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved(
    const std::string& data,
    bool status) {
  if (status) {
    VLOG(1) << "The Endpoint Verification data was retrieved.";
    Respond(OneArgument(std::make_unique<base::Value>(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
  } else {
    LogReportError("Endpoint Verification data retrieval error.");
    Respond(Error(enterprise_reporting::kEndpointVerificationRetrievalFailed));
  }
}

// setDeviceData

EnterpriseReportingPrivateSetDeviceDataFunction::
    EnterpriseReportingPrivateSetDeviceDataFunction() = default;
EnterpriseReportingPrivateSetDeviceDataFunction::
    ~EnterpriseReportingPrivateSetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateSetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::SetDeviceData::Params>
      params(api::enterprise_reporting_private::SetDeviceData::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &StoreDeviceData, params->id, std::move(params->data),
          base::BindOnce(
              &EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored,
              this)));
  return RespondLater();
}

void EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored(
    bool status) {
  if (status) {
    VLOG(1) << "The Endpoint Verification data was stored.";
    Respond(NoArguments());
  } else {
    LogReportError("Endpoint Verification data storage error.");
    Respond(Error(enterprise_reporting::kEndpointVerificationStoreFailed));
  }
}

// getDeviceInfo

EnterpriseReportingPrivateGetDeviceInfoFunction::
    EnterpriseReportingPrivateGetDeviceInfoFunction() = default;
EnterpriseReportingPrivateGetDeviceInfoFunction::
    ~EnterpriseReportingPrivateGetDeviceInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceInfoFunction::Run() {
#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&enterprise_reporting::DeviceInfoFetcher::Fetch,
                     enterprise_reporting::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#else
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateTaskRunner({base::MayBlock()}).get(), FROM_HERE,
      base::BindOnce(&enterprise_reporting::DeviceInfoFetcher::Fetch,
                     enterprise_reporting::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#endif  // defined(OS_WIN)

  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceInfoFunction::OnDeviceInfoRetrieved(
    const api::enterprise_reporting_private::DeviceInfo& device_info) {
  Respond(OneArgument(device_info.ToValue()));
}

}  // namespace extensions
