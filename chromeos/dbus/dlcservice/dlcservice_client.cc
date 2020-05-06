// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlcservice/dlcservice_client.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// TODO(kimjae): Delete and remove usages once system_api is uprev'ed.
const char kGetExistingDlcsMethod[] = "GetExistingDlcs";

DlcserviceClient* g_instance = nullptr;

class DlcserviceErrorResponseHandler {
 public:
  explicit DlcserviceErrorResponseHandler(dbus::ErrorResponse* err_response)
      : err_(dlcservice::kErrorInternal) {
    if (!err_response) {
      LOG(ERROR) << "Failed to set err since ErrorResponse is null.";
      return;
    }
    VerifyAndSetError(err_response);
    VerifyAndSetErrorMessage(err_response);
    VLOG(1) << "Handling err=" << err_ << " err_msg=" << err_msg_;
  }

  ~DlcserviceErrorResponseHandler() = default;

  std::string get_err() { return err_; }

  std::string get_err_msg() { return err_msg_; }

 private:
  void VerifyAndSetError(dbus::ErrorResponse* err_response) {
    const std::string& err = err_response->GetErrorName();
    static const base::NoDestructor<std::unordered_set<std::string>> err_set({
        dlcservice::kErrorNone,
        dlcservice::kErrorInternal,
        dlcservice::kErrorBusy,
        dlcservice::kErrorNeedReboot,
        dlcservice::kErrorInvalidDlc,
    });
    // Lookup the dlcservice error code and provide default on invalid.
    auto itr = err_set->find(err);
    if (itr == err_set->end()) {
      LOG(ERROR) << "Failed to set error based on ErrorResponse "
                    "defaulted to kErrorInternal, was:" << err;
      err_ = dlcservice::kErrorInternal;
      return;
    }
    err_ = *itr;
  }

  void VerifyAndSetErrorMessage(dbus::ErrorResponse* err_response) {
    if (!dbus::MessageReader(err_response).PopString(&err_msg_)) {
      LOG(ERROR) << "Failed to set error message from ErrorResponse.";
    }
  }

  // Holds the dlcservice specific error.
  std::string err_;

  // Holds the entire error message from error response.
  std::string err_msg_;

  DISALLOW_COPY_AND_ASSIGN(DlcserviceErrorResponseHandler);
};

}  // namespace

// The DlcserviceClient implementation used in production.
class DlcserviceClientImpl : public DlcserviceClient {
 public:
  DlcserviceClientImpl() : dlcservice_proxy_(nullptr) {}

  ~DlcserviceClientImpl() override = default;

  void Install(const dlcservice::DlcModuleList& dlc_module_list,
               InstallCallback install_callback,
               ProgressCallback progress_callback) override {
    if (!service_available_ || task_running_) {
      EnqueueTask(base::BindOnce(
          &DlcserviceClientImpl::Install, weak_ptr_factory_.GetWeakPtr(),
          std::move(dlc_module_list), std::move(install_callback),
          std::move(progress_callback)));
      return;
    }

    TaskStarted();
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kInstallMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(dlc_module_list);

    progress_callback_holder_ = std::move(progress_callback);
    install_callback_holder_ = std::move(install_callback);
    install_field_holder_ = dlc_module_list;

    VLOG(1) << "Requesting to install DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnInstall,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Uninstall(const std::string& dlc_id,
                 UninstallCallback uninstall_callback) override {
    if (!service_available_ || task_running_) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Uninstall,
                                 weak_ptr_factory_.GetWeakPtr(), dlc_id,
                                 std::move(uninstall_callback)));
      return;
    }

    TaskStarted();
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kUninstallMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    uninstall_callback_holder_ = std::move(uninstall_callback);
    uninstall_field_holder_ = dlc_id;

    VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnUninstall,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Purge(const std::string& dlc_id, PurgeCallback purge_callback) override {
    if (!service_available_ || task_running_) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Purge,
                                 weak_ptr_factory_.GetWeakPtr(), dlc_id,
                                 std::move(purge_callback)));
      return;
    }

    TaskStarted();
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kPurgeMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    purge_callback_holder_ = std::move(purge_callback);
    purge_field_holder_ = dlc_id;

    VLOG(1) << "Requesting to purge DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnPurge,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetInstalled(GetInstalledCallback callback) override {
    if (!service_available_ || task_running_) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::GetInstalled,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback)));
      return;
    }

    TaskStarted();
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kGetInstalledMethod);

    VLOG(1) << "Requesting to get installed DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnGetInstalled,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetExistingDlcs(GetExistingDlcsCallback callback) override {
    if (!service_available_ || task_running_) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::GetExistingDlcs,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback)));
      return;
    }

    TaskStarted();
    // TODO(kimjae): Use |kGetExistingDlcsMethod| once system_api is uprev'ed.
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 kGetExistingDlcsMethod);

    VLOG(1) << "Requesting to get existing DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnGetExistingDlcs,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnInstallStatusForTest(dbus::Signal* signal) override {
    OnInstallStatus(signal);
  }

  void Init(dbus::Bus* bus) {
    dlcservice_proxy_ = bus->GetObjectProxy(
        dlcservice::kDlcServiceServiceName,
        dbus::ObjectPath(dlcservice::kDlcServiceServicePath));
    dlcservice_proxy_->ConnectToSignal(
        dlcservice::kDlcServiceInterface, dlcservice::kOnInstallStatusSignal,
        base::BindRepeating(&DlcserviceClientImpl::OnInstallStatus,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DlcserviceClientImpl::OnInstallStatusConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Set the indication that an operation is being performed, which consists of
  // either |Install()|, |Uninstall()|, or |GetInstalled()|. Should always be
  // called after calling dlcservice in platform.
  void TaskStarted() { task_running_ = true; }

  // Clears any indication that an operation had setup while being performed,
  // which consists of either |Install()|, |Uninstall()|, or |GetInstalled()|.
  void TaskEnded() {
    task_running_ = false;
    // |Install()|
    install_callback_holder_.reset();
    progress_callback_holder_.reset();
    install_field_holder_.reset();
    // |Uninstall()|
    uninstall_callback_holder_.reset();
    uninstall_field_holder_.reset();
    // |Purge()|
    purge_callback_holder_.reset();
    purge_field_holder_.reset();
  }

  void EnqueueTask(base::OnceClosure task) {
    pending_tasks_.emplace_back(std::move(task));
  }

  void CheckAndRunPendingTask() {
    TaskEnded();
    if (!pending_tasks_.empty()) {
      std::move(pending_tasks_.front()).Run();
      pending_tasks_.pop_front();
    }
  }

  void SendProgress(const dlcservice::InstallStatus& install_status) {
    const double progress = install_status.progress();
    VLOG(2) << "Install in progress: " << progress;
    if (progress_callback_holder_.has_value())
      progress_callback_holder_.value().Run(progress);
  }

  void SendCompleted(const dlcservice::InstallStatus& install_status) {
    std::move(install_callback_holder_.value())
        .Run(install_status.error_code(), install_status.dlc_module_list());
  }

  void OnInstallStatus(dbus::Signal* signal) {
    if (!install_callback_holder_.has_value())
      return;

    dlcservice::InstallStatus install_status;
    if (!dbus::MessageReader(signal).PopArrayOfBytesAsProto(&install_status)) {
      LOG(ERROR) << "Failed to parse proto as install status.";
      return;
    }

    switch (install_status.status()) {
      case dlcservice::Status::COMPLETED:
        VLOG(1) << "DLC(s) install successful.";
        SendCompleted(install_status);
        break;
      case dlcservice::Status::RUNNING: {
        SendProgress(install_status);
        // Need to return here since we don't want to try starting another
        // pending install from the queue (would waste time checking).
        return;
      }
      case dlcservice::Status::FAILED:
        LOG(ERROR) << "Failed to install with error code: "
                   << install_status.error_code();
        SendCompleted(install_status);
        break;
      default:
        NOTREACHED();
    }

    // Try to run a pending install since we have complete/failed the current
    // install, but do not waste trying to run a pending install when the
    // current install is running at the moment.
    CheckAndRunPendingTask();
  }

  void OnInstallStatusConnected(const std::string& interface,
                                const std::string& signal,
                                bool success) {
    // When the connected to dlcservice daemon's |OnInstallStatus| signal we can
    // go ahead and mark the service as being available and not queue up tasks
    // that came in before dlcservice daemon was available.
    if (success) {
      service_available_ = true;
      CheckAndRunPendingTask();
    } else {
      LOG(ERROR) << "Failed to connect to install status signal.";
      pending_tasks_.clear();
    }
  }

  void OnInstall(dbus::Response* response, dbus::ErrorResponse* err_response) {
    if (response)
      return;

    // Perform DCHECKs only when an error occurs, platform dlcservice currently
    // sends a signal prior to DBus method callback on quick install scenarios.
    DCHECK(install_field_holder_.has_value());
    DCHECK(install_callback_holder_.has_value());
    DCHECK(progress_callback_holder_.has_value());

    const auto err = DlcserviceErrorResponseHandler(err_response).get_err();
    if (err == dlcservice::kErrorBusy) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Install,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(install_field_holder_.value()),
                                 std::move(install_callback_holder_.value()),
                                 std::move(progress_callback_holder_.value())));
    } else {
      dlcservice::InstallStatus install_status;
      install_status.set_error_code(err);
      SendCompleted(install_status);
    }
    CheckAndRunPendingTask();
  }

  void OnUninstall(dbus::Response* response,
                   dbus::ErrorResponse* err_response) {
    DCHECK(uninstall_field_holder_.has_value());
    DCHECK(uninstall_callback_holder_.has_value());
    if (response) {
      std::move(uninstall_callback_holder_.value()).Run(dlcservice::kErrorNone);
    } else {
      const auto err = DlcserviceErrorResponseHandler(err_response).get_err();
      if (err == dlcservice::kErrorBusy) {
        EnqueueTask(base::BindOnce(
            &DlcserviceClientImpl::Uninstall, weak_ptr_factory_.GetWeakPtr(),
            std::move(uninstall_field_holder_.value()),
            std::move(uninstall_callback_holder_.value())));
      } else {
        std::move(uninstall_callback_holder_.value()).Run(err);
      }
    }
    CheckAndRunPendingTask();
  }

  void OnPurge(dbus::Response* response, dbus::ErrorResponse* err_response) {
    DCHECK(purge_field_holder_.has_value());
    DCHECK(purge_callback_holder_.has_value());
    if (response) {
      std::move(purge_callback_holder_.value()).Run(dlcservice::kErrorNone);
    } else {
      const auto err = DlcserviceErrorResponseHandler(err_response).get_err();
      if (err == dlcservice::kErrorBusy) {
        EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Purge,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(purge_field_holder_.value()),
                                   std::move(purge_callback_holder_.value())));
      } else {
        std::move(purge_callback_holder_.value()).Run(err);
      }
    }
    CheckAndRunPendingTask();
  }

  void OnGetInstalled(GetInstalledCallback callback,
                      dbus::Response* response,
                      dbus::ErrorResponse* err_response) {
    dlcservice::DlcModuleList dlc_module_list;
    if (response && dbus::MessageReader(response).PopArrayOfBytesAsProto(
                        &dlc_module_list)) {
      std::move(callback).Run(dlcservice::kErrorNone, dlc_module_list);
    } else {
      std::move(callback).Run(
          DlcserviceErrorResponseHandler(err_response).get_err(),
          dlcservice::DlcModuleList());
    }
    CheckAndRunPendingTask();
  }

  void OnGetExistingDlcs(GetExistingDlcsCallback callback,
                         dbus::Response* response,
                         dbus::ErrorResponse* err_response) {
    dlcservice::DlcsWithContent dlcs_with_content;
    if (response && dbus::MessageReader(response).PopArrayOfBytesAsProto(
                        &dlcs_with_content)) {
      std::move(callback).Run(dlcservice::kErrorNone, dlcs_with_content);
    } else {
      std::move(callback).Run(
          DlcserviceErrorResponseHandler(err_response).get_err(),
          dlcservice::DlcsWithContent());
    }
    CheckAndRunPendingTask();
  }

  dbus::ObjectProxy* dlcservice_proxy_;

  // True after dlcservice's D-Bus service has become available.
  bool service_available_ = false;

  // Whether any task is currently in progress. Can be used to decide whether to
  // queue up incoming requests.
  bool task_running_ = false;

  // The cached callback to call on a finished |Install()|.
  base::Optional<InstallCallback> install_callback_holder_;

  // The cached callback to call on during progress of |Install()|.
  base::Optional<ProgressCallback> progress_callback_holder_;

  // The cached callback to call on a finished |Uninstall()|.
  base::Optional<UninstallCallback> uninstall_callback_holder_;

  // The cached callback to call on a finished |Uninstall()|.
  base::Optional<PurgeCallback> purge_callback_holder_;

  // The cached field of |DlcModuleList| for retrying call to install.
  base::Optional<dlcservice::DlcModuleList> install_field_holder_;

  // The cached field of string (DLC ID) for retrying call to uninstall.
  base::Optional<std::string> uninstall_field_holder_;

  // The cached field of string (DLC ID) for retrying call to purge.
  base::Optional<std::string> purge_field_holder_;

  // A list of postponed calls to dlcservice to be called after it becomes
  // available or after the currently running task completes.
  std::deque<base::OnceClosure> pending_tasks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DlcserviceClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DlcserviceClientImpl);
};

const DlcserviceClient::ProgressCallback DlcserviceClient::IgnoreProgress =
    base::BindRepeating([](double) {});

DlcserviceClient::DlcserviceClient() {
  CHECK(!g_instance);
  g_instance = this;
}

DlcserviceClient::~DlcserviceClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void DlcserviceClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new DlcserviceClientImpl())->Init(bus);
}

// static
void DlcserviceClient::InitializeFake() {
  new FakeDlcserviceClient();
}

// static
void DlcserviceClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
DlcserviceClient* DlcserviceClient::Get() {
  return g_instance;
}

}  // namespace chromeos
