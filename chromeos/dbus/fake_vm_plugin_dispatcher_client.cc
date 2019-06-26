// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

using namespace vm_tools::plugin_dispatcher;

namespace chromeos {

FakeVmPluginDispatcherClient::FakeVmPluginDispatcherClient() = default;

FakeVmPluginDispatcherClient::~FakeVmPluginDispatcherClient() = default;

void FakeVmPluginDispatcherClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeVmPluginDispatcherClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeVmPluginDispatcherClient::StartVm(
    const StartVmRequest& request,
    DBusMethodCallback<StartVmResponse> callback) {
  start_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeVmPluginDispatcherClient::ListVms(
    const ListVmRequest& request,
    DBusMethodCallback<ListVmResponse> callback) {
  list_vms_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vms_response_));
}

void FakeVmPluginDispatcherClient::StopVm(
    const StopVmRequest& request,
    DBusMethodCallback<StopVmResponse> callback) {
  stop_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), StopVmResponse()));
}

void FakeVmPluginDispatcherClient::SuspendVm(
    const SuspendVmRequest& request,
    DBusMethodCallback<SuspendVmResponse> callback) {
  suspend_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SuspendVmResponse()));
}

void FakeVmPluginDispatcherClient::ShowVm(
    const ShowVmRequest& request,
    DBusMethodCallback<ShowVmResponse> callback) {
  show_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ShowVmResponse()));
}

void FakeVmPluginDispatcherClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeVmPluginDispatcherClient::NotifyVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnVmStateChanged(signal);
  }
}

}  // namespace chromeos
