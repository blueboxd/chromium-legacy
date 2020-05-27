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

  void Install(const std::string& dlc_id,
               InstallCallback install_callback,
               ProgressCallback progress_callback) override {
    if (installing_) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Install,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(dlc_id), std::move(install_callback),
                                 std::move(progress_callback)));
      return;
    }

    TaskStarted();
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kInstallDlcMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    progress_callback_holder_ = std::move(progress_callback);
    install_callback_holder_ = std::move(install_callback);
    dlc_id_holder_ = dlc_id;

    VLOG(1) << "Requesting to install DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnInstall,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Uninstall(const std::string& dlc_id,
                 UninstallCallback uninstall_callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kUninstallMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnUninstall,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(uninstall_callback)));
  }

  void Purge(const std::string& dlc_id, PurgeCallback purge_callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kPurgeMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    VLOG(1) << "Requesting to purge DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnPurge,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(purge_callback)));
  }

  void GetExistingDlcs(GetExistingDlcsCallback callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kGetExistingDlcsMethod);

    VLOG(1) << "Requesting to get existing DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnGetExistingDlcs,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DlcStateChangedForTest(dbus::Signal* signal) override {
    DlcStateChanged(signal);
  }

  void Init(dbus::Bus* bus) {
    dlcservice_proxy_ = bus->GetObjectProxy(
        dlcservice::kDlcServiceServiceName,
        dbus::ObjectPath(dlcservice::kDlcServiceServicePath));
    dlcservice_proxy_->ConnectToSignal(
        dlcservice::kDlcServiceInterface, dlcservice::kDlcStateChangedSignal,
        base::BindRepeating(&DlcserviceClientImpl::DlcStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DlcserviceClientImpl::DlcStateChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Set the indication that an install is being performed which was requested
  // from this client (Chrome specifically).
  void TaskStarted() { installing_ = true; }

  // Clears any state an installation had setup while being performed.
  void TaskEnded() {
    installing_ = false;
    // |Install()|
    install_callback_holder_.reset();
    progress_callback_holder_.reset();
    dlc_id_holder_.reset();
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

  void SendProgress(double progress) {
    VLOG(2) << "Install in progress: " << progress;
    if (progress_callback_holder_.has_value())
      progress_callback_holder_.value().Run(progress);
  }

  void SendCompleted(const dlcservice::DlcState& dlc_state) {
    if (!install_callback_holder_.has_value() || !dlc_id_holder_.has_value())
      return;

    if (dlc_id_holder_ != dlc_state.id()) {
      // We are not interested in this DLC. Some other process might be
      // installing it.
      return;
    }

    if (dlc_state.state() == dlcservice::DlcState::NOT_INSTALLED) {
      LOG(ERROR) << "Failed to install DLC " << dlc_state.id()
                 << " with error code: " << dlc_state.last_error_code();

    } else {
      VLOG(1) << "DLC " << dlc_state.id() << " installed successfully.";
      if (dlc_state.last_error_code() != dlcservice::kErrorNone) {
        LOG(WARNING) << "DLC installation was sucessful but non-success "
                     << "error code: " << dlc_state.last_error_code();
      }
    }

    InstallResult result = {
        .error = dlc_state.last_error_code(),
        .dlc_id = dlc_state.id(),
        .root_path = dlc_state.root_path(),
    };
    std::move(install_callback_holder_.value()).Run(result);
  }

  void DlcStateChanged(dbus::Signal* signal) {
    dlcservice::DlcState dlc_state;
    if (!dbus::MessageReader(signal).PopArrayOfBytesAsProto(&dlc_state)) {
      LOG(ERROR) << "Failed to parse proto as install status.";
      return;
    }

    switch (dlc_state.state()) {
      case dlcservice::DlcState::NOT_INSTALLED:
      case dlcservice::DlcState::INSTALLED:
        SendCompleted(dlc_state);
        break;
      case dlcservice::DlcState::INSTALLING:
        SendProgress(dlc_state.progress());
        // Need to return here since we don't want to try starting another
        // pending install from the queue (would waste time checking).
        return;
      default:
        NOTREACHED();
    }

    // Try to run a pending install since we have complete/failed the current
    // install, but do not waste trying to run a pending install when the
    // current install is running at the moment.
    CheckAndRunPendingTask();
  }

  void DlcStateChangedConnected(const std::string& interface,
                                const std::string& signal,
                                bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to DlcStateChanged signal.";
  }

  void OnInstall(dbus::Response* response, dbus::ErrorResponse* err_response) {
    if (response)
      return;

    // Perform DCHECKs only when an error occurs, platform dlcservice currently
    // sends a signal prior to DBus method callback on quick install scenarios.
    DCHECK(dlc_id_holder_.has_value());
    DCHECK(install_callback_holder_.has_value());
    DCHECK(progress_callback_holder_.has_value());

    const auto err = DlcserviceErrorResponseHandler(err_response).get_err();
    if (err == dlcservice::kErrorBusy) {
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Install,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(dlc_id_holder_.value()),
                                 std::move(install_callback_holder_.value()),
                                 std::move(progress_callback_holder_.value())));
    } else {
      dlcservice::DlcState dlc_state;
      dlc_state.set_id(dlc_id_holder_.value());
      dlc_state.set_last_error_code(err);
      SendCompleted(dlc_state);
    }
    CheckAndRunPendingTask();
  }

  void OnUninstall(UninstallCallback uninstall_callback,
                   dbus::Response* response,
                   dbus::ErrorResponse* err_response) {
    std::move(uninstall_callback)
        .Run(response ? dlcservice::kErrorNone
                      : DlcserviceErrorResponseHandler(err_response).get_err());
  }

  void OnPurge(PurgeCallback purge_callback,
               dbus::Response* response,
               dbus::ErrorResponse* err_response) {
    std::move(purge_callback)
        .Run(response ? dlcservice::kErrorNone
                      : DlcserviceErrorResponseHandler(err_response).get_err());
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
  }

  dbus::ObjectProxy* dlcservice_proxy_;

  // Whether an install is currently in progress. Can be used to decide whether
  // to queue up incoming install requests.
  bool installing_ = false;

  // The cached callback to call on a finished |Install()|.
  base::Optional<InstallCallback> install_callback_holder_;

  // The cached callback to call on during progress of |Install()|.
  base::Optional<ProgressCallback> progress_callback_holder_;

  // The cached DLC ID for retrying call to install.
  base::Optional<std::string> dlc_id_holder_;

  // A list of postponed installs to dlcservice.
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
