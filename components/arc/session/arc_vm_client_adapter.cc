// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "components/arc/arc_util.h"

namespace arc {

namespace {

constexpr char kHomeDirectory[] = "/home";
constexpr char kKernelPath[] = "/opt/google/vms/android/vmlinux";
constexpr char kRootFsPath[] = "/opt/google/vms/android/system.raw.img";
constexpr char kVendorImagePath[] = "/opt/google/vms/android/vendor.raw.img";

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

std::vector<std::string> CreateKernelParams() {
  // TODO(cmtm): Generate the same parameters as arc-setup based on both
  // StartArcMiniContainerRequest and UpgradeArcContainerRequest.

  // TODO(yusukes): Enable SELinux.
  std::vector<std::string> params{
      {"androidboot.selinux=permissive"},
      {"androidboot.hardware=bertha"},
      {"androidboot.debuggable=1"},
      {"androidboot.native_bridge=libhoudini.so"},
  };
  return params;
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter {
 public:
  ArcVmClientAdapter() : weak_factory_(this) {}
  ~ArcVmClientAdapter() override = default;

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(yusukes): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";
    base::PostTask(FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(1) << "Starting Concierge service";
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
        base::BindOnce(&ArcVmClientAdapter::OnConciergeStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       CreateKernelParams()));
  }

  void StopArcInstance() override {
    VLOG(1) << "Stopping arcvm";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnVmStopped,
                                weak_factory_.GetWeakPtr()));
  }

  void SetUserIdHashForProfile(const std::string& hash) override {
    DCHECK(!hash.empty());
    user_id_hash_ = hash;
  }

 private:
  // TODO(yusukes|cmtm): Monitor unexpected crosvm crash/shutdown and call this
  // method.
  void OnArcInstanceStopped() {
    VLOG(1) << "arcvm stopped.";
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void OnConciergeStarted(chromeos::VoidDBusMethodCallback callback,
                          std::vector<std::string> params,
                          bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to start Concierge service for arcvm";
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Concierge service started for arcvm.";

    // TODO(cmtm): Export host-side /data to the VM, and remove the call.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                       base::FilePath(kHomeDirectory)),
        base::BindOnce(&ArcVmClientAdapter::CreateDiskImageAfterSizeCheck,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(params)));
  }

  // TODO(cmtm): Export host-side /data to the VM, and remove the function.
  void CreateDiskImageAfterSizeCheck(chromeos::VoidDBusMethodCallback callback,
                                     std::vector<std::string> params,
                                     int64_t free_disk_bytes) {
    VLOG(2) << "Got free disk size: " << free_disk_bytes;
    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      std::move(callback).Run(false);
      return;
    }

    vm_tools::concierge::CreateDiskImageRequest request;
    request.set_cryptohome_id(user_id_hash_);
    request.set_disk_path(kArcVmName);
    // The type of disk image to be created.
    request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    // The logical size of the new disk image, in bytes.
    request.set_disk_size(free_disk_bytes / 2);

    GetConciergeClient()->CreateDiskImage(
        std::move(request),
        base::BindOnce(&ArcVmClientAdapter::OnDiskImageCreated,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(params)));
  }

  void OnDiskImageCreated(
      chromeos::VoidDBusMethodCallback callback,
      std::vector<std::string> params,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to create disk image. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::CreateDiskImageResponse& response =
        reply.value();
    if (response.status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
        response.status() != vm_tools::concierge::DISK_STATUS_CREATED) {
      LOG(ERROR) << "Failed to create disk image: "
                 << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Disk image for arcvm created. status=" << response.status()
            << ", disk=" << response.disk_path();

    DCHECK(!user_id_hash_.empty());
    vm_tools::concierge::StartArcVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);

    request.add_params("root=/dev/vda");
    // TODO(b/135229848): Use ro unless rw is requested.
    request.add_params("rw");
    request.add_params("init=/init");
    for (auto& param : params)
      request.add_params(std::move(param));

    vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();
    vm->set_kernel(kKernelPath);
    // Add / as /dev/vda.
    vm->set_rootfs(kRootFsPath);

    // Add /data as /dev/vdb.
    vm_tools::concierge::DiskImage* disk_image = request.add_disks();
    disk_image->set_path(response.disk_path());
    disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    disk_image->set_writable(true);
    disk_image->set_do_mount(true);
    // Add /vendor as /dev/vdc.
    disk_image = request.add_disks();
    disk_image->set_path(kVendorImagePath);
    disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    disk_image->set_writable(false);
    disk_image->set_do_mount(true);

    GetConciergeClient()->StartArcVm(
        request,
        base::BindOnce(&ArcVmClientAdapter::OnVmStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnVmStarted(chromeos::VoidDBusMethodCallback callback,
                   base::Optional<vm_tools::concierge::StartVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to start arcvm. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::StartVmResponse& response = reply.value();
    if (response.status() != vm_tools::concierge::VM_STATUS_RUNNING) {
      LOG(ERROR) << "Failed to start arcvm: status=" << response.status()
                 << ", reason=" << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "arcvm started.";
    std::move(callback).Run(true);
    // TODO(yusukes): Share folders like Downloads/ with ARCVM.
  }

  void OnVmStopped(base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to stop arcvm. Empty response.";
    } else {
      const vm_tools::concierge::StopVmResponse& response = reply.value();
      if (!response.success())
        LOG(ERROR) << "Failed to stop arcvm: " << response.failure_reason();
    }
    OnArcInstanceStopped();
  }

  // A hash of the primary profile user ID.
  std::string user_id_hash_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

}  // namespace arc
