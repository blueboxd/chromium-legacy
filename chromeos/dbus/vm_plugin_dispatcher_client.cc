// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/vm_plugin_dispatcher_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_plugin_dispatcher/dbus-constants.h"

namespace chromeos {

using namespace vm_tools::plugin_dispatcher;

class VmPluginDispatcherClientImpl : public VmPluginDispatcherClient {
 public:
  VmPluginDispatcherClientImpl() {}

  ~VmPluginDispatcherClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void StartVm(const StartVmRequest& request,
               DBusMethodCallback<StartVmResponse> callback) override {
    CallMethod(kStartVmMethod, request, std::move(callback));
  }

  void ListVms(const ListVmRequest& request,
               DBusMethodCallback<ListVmResponse> callback) override {
    CallMethod(kListVmsMethod, request, std::move(callback));
  }

  void StopVm(const StopVmRequest& request,
              DBusMethodCallback<StopVmResponse> callback) override {
    CallMethod(kStopVmMethod, request, std::move(callback));
  }

  void SuspendVm(const SuspendVmRequest& request,
                 DBusMethodCallback<SuspendVmResponse> callback) override {
    CallMethod(kSuspendVmMethod, request, std::move(callback));
  }

  void ShowVm(const ShowVmRequest& request,
              DBusMethodCallback<ShowVmResponse> callback) override {
    CallMethod(kShowVmMethod, request, std::move(callback));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    vm_plugin_dispatcher_proxy_->WaitForServiceToBeAvailable(
        std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    vm_plugin_dispatcher_proxy_ =
        bus->GetObjectProxy(kVmPluginDispatcherServiceName,
                            dbus::ObjectPath(kVmPluginDispatcherServicePath));
    if (!vm_plugin_dispatcher_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << kVmPluginDispatcherServiceName;
    }

    vm_plugin_dispatcher_proxy_->ConnectToSignal(
        vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
        vm_tools::plugin_dispatcher::kVmStateChangedSignal,
        base::BindRepeating(
            &VmPluginDispatcherClientImpl::OnVmStateChangedSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&VmPluginDispatcherClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  template <typename RequestProto, typename ResponseProto>
  void CallMethod(const std::string& method_name,
                  const RequestProto& request,
                  DBusMethodCallback<ResponseProto> callback) {
    dbus::MethodCall method_call(kVmPluginDispatcherInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    vm_plugin_dispatcher_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &VmPluginDispatcherClientImpl::OnDBusProtoResponse<ResponseProto>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  template <typename ResponseProto>
  void OnDBusProtoResponse(DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  void OnVmStateChangedSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(),
              vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface);
    DCHECK_EQ(signal->GetMember(),
              vm_tools::plugin_dispatcher::kVmStateChangedSignal);

    vm_tools::plugin_dispatcher::VmStateChangedSignal vm_state_changed_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&vm_state_changed_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : observer_list_) {
      observer.OnVmStateChanged(vm_state_changed_signal);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    DCHECK_EQ(interface_name,
              vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface);
    if (!is_connected)
      LOG(ERROR) << "Failed to connect to signal: " << signal_name;
  }

  dbus::ObjectProxy* vm_plugin_dispatcher_proxy_ = nullptr;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<VmPluginDispatcherClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VmPluginDispatcherClientImpl);
};

VmPluginDispatcherClient::VmPluginDispatcherClient() = default;

VmPluginDispatcherClient::~VmPluginDispatcherClient() = default;

std::unique_ptr<VmPluginDispatcherClient> VmPluginDispatcherClient::Create() {
  return std::make_unique<VmPluginDispatcherClientImpl>();
}

}  // namespace chromeos
