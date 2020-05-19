// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

// Returns localized representation for the state of a certificate provisioning
// process.
base::string16 GetProvisioningProcessStatus(
    chromeos::cert_provisioning::CertProvisioningWorkerState state) {
  using CertProvisioningWorkerState =
      chromeos::cert_provisioning::CertProvisioningWorkerState;
  switch (state) {
    case CertProvisioningWorkerState ::kInitState:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningWorkerState ::kKeypairGenerated:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kVaChallengeFinished:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kKeyRegistered:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kKeypairMarked:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningWorkerState::kSignCsrFinished:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_WAITING_FOR_CA);
    case CertProvisioningWorkerState::kSucceeded:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_SUCCESS);
    case CertProvisioningWorkerState::kFailed:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE);
    case CertProvisioningWorkerState::kInconsistentDataError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kCanceled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_CANCELED);
  }
  NOTREACHED();
}

// Returns a localized representation of the last update time as a delay (e.g.
// "5 minutes ago".
base::string16 GetTimeSinceLastUpdate(base::Time last_update_time) {
  const base::Time now = base::Time::NowFromSystemTime();
  if (last_update_time.is_null() || last_update_time > now)
    return base::string16();
  const base::TimeDelta elapsed_time = now - last_update_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

base::Value CreateProvisioningProcessEntry(
    const std::string& cert_profile_id,
    bool is_device_wide,
    chromeos::cert_provisioning::CertProvisioningWorkerState state,
    base::Time time_since_last_update,
    const std::string& public_key_spki_der) {
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetStringKey("certProfileId", cert_profile_id);
  entry.SetBoolKey("isDeviceWide", is_device_wide);
  entry.SetStringKey("status", GetProvisioningProcessStatus(state));
  entry.SetIntKey("stateId", static_cast<int>(state));
  entry.SetStringKey("timeSinceLastUpdate",
                     GetTimeSinceLastUpdate(time_since_last_update));

  auto spki_der_bytes = base::as_bytes(base::make_span(public_key_spki_der));
  entry.SetStringKey(
      "publicKey",
      x509_certificate_model::ProcessRawSubjectPublicKeyInfo(spki_der_bytes));

  return entry;
}

// Collects information about certificate provisioning processes from
// |cert_provisioning_scheduler| and appends them to |list_to_append_to|.
void CollectProvisioningProcesses(
    base::Value* list_to_append_to,
    CertProvisioningScheduler* cert_provisioning_scheduler,
    bool is_device_wide) {
  for (const auto& worker_entry : cert_provisioning_scheduler->GetWorkers()) {
    CertProvisioningWorker* worker = worker_entry.second.get();
    list_to_append_to->Append(CreateProvisioningProcessEntry(
        worker_entry.first, is_device_wide, worker->GetState(),
        worker->GetLastUpdateTime(), worker->GetPublicKey()));
  }
  for (const auto& failed_worker_entry :
       cert_provisioning_scheduler->GetFailedCertProfileIds()) {
    const chromeos::cert_provisioning::FailedWorkerInfo& worker =
        failed_worker_entry.second;
    list_to_append_to->Append(CreateProvisioningProcessEntry(
        failed_worker_entry.first, is_device_wide, worker.state,
        worker.last_update_time, worker.public_key));
  }
}

}  // namespace

CertificateProvisioningUiHandler::CertificateProvisioningUiHandler() = default;
CertificateProvisioningUiHandler::~CertificateProvisioningUiHandler() = default;

void CertificateProvisioningUiHandler::RegisterMessages() {
  // Passing base::Unretained(this) to web_ui()->RegisterMessageCallback is fine
  // because in chrome Web UI, web_ui() has acquired ownership of |this| and
  // maintains the life time of |this| accordingly.
  web_ui()->RegisterMessageCallback(
      "refreshCertificateProvisioningProcessses",
      base::BindRepeating(&CertificateProvisioningUiHandler::
                              HandleRefreshCertificateProvisioningProcesses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "triggerCertificateProvisioningProcessUpdate",
      base::BindRepeating(&CertificateProvisioningUiHandler::
                              HandleTriggerCertificateProvisioningProcessUpdate,
                          base::Unretained(this)));
}

CertProvisioningScheduler*
CertificateProvisioningUiHandler::GetCertProvisioningSchedulerForUser(
    Profile* user_profile) {
  chromeos::cert_provisioning::CertProvisioningSchedulerUserService*
      user_service = chromeos::cert_provisioning::
          CertProvisioningSchedulerUserServiceFactory::GetForProfile(
              user_profile);
  if (!user_service)
    return nullptr;
  return user_service->scheduler();
}

CertProvisioningScheduler*
CertificateProvisioningUiHandler::GetCertProvisioningSchedulerForDevice(
    Profile* user_profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(user_profile);
  if (!user || !user->IsAffiliated())
    return nullptr;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetDeviceCertProvisioningScheduler();
}

void CertificateProvisioningUiHandler::
    HandleRefreshCertificateProvisioningProcesses(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  AllowJavascript();
  RefreshCertificateProvisioningProcesses();
}

void CertificateProvisioningUiHandler::
    HandleTriggerCertificateProvisioningProcessUpdate(
        const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  if (!args->is_list())
    return;
  const base::Value& cert_profile_id = args->GetList()[0];
  if (!cert_profile_id.is_string())
    return;
  const base::Value& device_wide = args->GetList()[1];
  if (!device_wide.is_bool())
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  CertProvisioningScheduler* scheduler =
      device_wide.GetBool() ? GetCertProvisioningSchedulerForDevice(profile)
                            : GetCertProvisioningSchedulerForUser(profile);
  if (!scheduler)
    return;

  scheduler->UpdateOneCert(cert_profile_id.GetString());

  // Send an update to the UI immediately to reflect a possible status change.
  RefreshCertificateProvisioningProcesses();

  // Trigger a refresh in a few seconds, in case the state has triggered a
  // refresh with the server.
  // TODO(https://crbug.com/1045895): Use a real observer instead.
  constexpr base::TimeDelta kTimeToWaitBeforeRefresh =
      base::TimeDelta::FromSeconds(10);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertificateProvisioningUiHandler::
                         RefreshCertificateProvisioningProcesses,
                     weak_ptr_factory_.GetWeakPtr()),
      kTimeToWaitBeforeRefresh);
}

void CertificateProvisioningUiHandler::
    RefreshCertificateProvisioningProcesses() {
  Profile* profile = Profile::FromWebUI(web_ui());

  base::ListValue all_processes;
  CertProvisioningScheduler* scheduler_for_user =
      GetCertProvisioningSchedulerForUser(profile);
  if (scheduler_for_user)
    CollectProvisioningProcesses(&all_processes, scheduler_for_user,
                                 /*is_device_wide=*/false);

  CertProvisioningScheduler* scheduler_for_device =
      GetCertProvisioningSchedulerForDevice(profile);
  if (scheduler_for_device)
    CollectProvisioningProcesses(&all_processes, scheduler_for_device,
                                 /*is_device_wide=*/true);

  FireWebUIListener("certificate-provisioning-processes-changed",
                    std::move(all_processes));
}

}  // namespace cert_provisioning
}  // namespace chromeos
