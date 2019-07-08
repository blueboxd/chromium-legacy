// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/concierge_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_concierge/dbus-constants.h"

namespace chromeos {

class ConciergeClientImpl : public ConciergeClient {
 public:
  ConciergeClientImpl() : weak_ptr_factory_(this) {}

  ~ConciergeClientImpl() override = default;

  void AddContainerObserver(ContainerObserver* observer) override {
    container_observer_list_.AddObserver(observer);
  }

  void RemoveContainerObserver(ContainerObserver* observer) override {
    container_observer_list_.RemoveObserver(observer);
  }

  void AddDiskImageObserver(DiskImageObserver* observer) override {
    disk_image_observer_list_.AddObserver(observer);
  }

  void RemoveDiskImageObserver(DiskImageObserver* observer) override {
    disk_image_observer_list_.RemoveObserver(observer);
  }

  bool IsContainerStartupFailedSignalConnected() override {
    return is_container_startup_failed_signal_connected_;
  }

  bool IsDiskImageProgressSignalConnected() override {
    return is_disk_import_progress_signal_connected_;
  }

  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kCreateDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CreateDiskImageRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::CreateDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kDestroyDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode DestroyDiskImageRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::DestroyDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ImportDiskImage(
      base::ScopedFD fd,
      const vm_tools::concierge::ImportDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kImportDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ImportDiskImageRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    writer.AppendFileDescriptor(fd.get());

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::ImportDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CancelDiskImageOperation(
      const vm_tools::concierge::CancelDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kCancelDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelDiskImageRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::CancelDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DiskImageStatus(
      const vm_tools::concierge::DiskImageStatusRequest& request,
      DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kDiskImageStatusMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode DiskImageStatusRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::DiskImageStatusResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ListVmDisks(const vm_tools::concierge::ListVmDisksRequest& request,
                   DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
                       callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kListVmDisksMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ListVmDisksRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::ListVmDisksResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStartVmMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartVmRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    // TODO(nverne): revert to TIMEOUT_USE_DEFAULT when StartVm no longer
    // requires unnecessary long running crypto calculations.
    constexpr int kStartVmTimeoutMs = 160 * 1000;
    concierge_proxy_->CallMethod(
        &method_call, kStartVmTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StartVmResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStopVmMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StopVmRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StopVmResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetVmInfo(const vm_tools::concierge::GetVmInfoRequest& request,
                 DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse>
                     callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kGetVmInfoMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetVmInfoRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::GetVmInfoResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetVmEnterpriseReportingInfo(
      const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
      DBusMethodCallback<
          vm_tools::concierge::GetVmEnterpriseReportingInfoResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::concierge::kVmConciergeInterface,
        vm_tools::concierge::kGetVmEnterpriseReportingInfoMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR)
          << "Failed to encode GetVmEnterpriseReportingInfoRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &ConciergeClientImpl::OnDBusProtoResponse<
                vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    concierge_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void GetContainerSshKeys(
      const vm_tools::concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
          callback) override {
    dbus::MethodCall method_call(
        vm_tools::concierge::kVmConciergeInterface,
        vm_tools::concierge::kGetContainerSshKeysMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ContainerSshKeysRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::ContainerSshKeysResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AttachUsbDevice(base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kAttachUsbDeviceMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode AttachUsbDeviceRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    writer.AppendFileDescriptor(fd.get());

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::AttachUsbDeviceResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kDetachUsbDeviceMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode DetachUsbDeviceRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::DetachUsbDeviceResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ListUsbDevices(
      const vm_tools::concierge::ListUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::ListUsbDeviceResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kListUsbDeviceMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ListUsbDeviceRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::ListUsbDeviceResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartArcVm(const vm_tools::concierge::StartArcVmRequest& request,
                  DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                      callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStartArcVmMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartArcVmRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StartVmResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    concierge_proxy_ = bus->GetObjectProxy(
        vm_tools::concierge::kVmConciergeServiceName,
        dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
    if (!concierge_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << vm_tools::concierge::kVmConciergeServiceName;
    }
    concierge_proxy_->ConnectToSignal(
        vm_tools::concierge::kVmConciergeInterface,
        vm_tools::concierge::kContainerStartupFailedSignal,
        base::BindRepeating(
            &ConciergeClientImpl::OnContainerStartupFailedSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        vm_tools::concierge::kVmConciergeInterface,
        vm_tools::concierge::kDiskImageProgressSignal,
        base::BindRepeating(&ConciergeClientImpl::OnDiskImageProgress,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
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

  void OnContainerStartupFailedSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(),
              vm_tools::concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(),
              vm_tools::concierge::kContainerStartupFailedSignal);

    vm_tools::concierge::ContainerStartedSignal container_startup_failed_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&container_startup_failed_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : container_observer_list_) {
      observer.OnContainerStartupFailed(container_startup_failed_signal);
    }
  }

  void OnDiskImageProgress(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(),
              vm_tools::concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(),
              vm_tools::concierge::kDiskImageProgressSignal);

    vm_tools::concierge::DiskImageStatusResponse
        disk_image_status_response_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&disk_image_status_response_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : disk_image_observer_list_) {
      observer.OnDiskImageProgress(disk_image_status_response_signal);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    DCHECK_EQ(interface_name, vm_tools::concierge::kVmConciergeInterface);
    if (!is_connected)
      LOG(ERROR) << "Failed to connect to signal: " << signal_name;

    if (signal_name == vm_tools::concierge::kContainerStartupFailedSignal) {
      is_container_startup_failed_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::concierge::kDiskImageProgressSignal) {
      is_disk_import_progress_signal_connected_ = is_connected;
    } else {
      NOTREACHED();
    }
  }

  dbus::ObjectProxy* concierge_proxy_ = nullptr;

  base::ObserverList<ContainerObserver>::Unchecked container_observer_list_;
  base::ObserverList<DiskImageObserver>::Unchecked disk_image_observer_list_;

  bool is_container_startup_failed_signal_connected_ = false;
  bool is_disk_import_progress_signal_connected_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ConciergeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConciergeClientImpl);
};

ConciergeClient::ConciergeClient() = default;

ConciergeClient::~ConciergeClient() = default;

std::unique_ptr<ConciergeClient> ConciergeClient::Create() {
  return std::make_unique<ConciergeClientImpl>();
}

}  // namespace chromeos
