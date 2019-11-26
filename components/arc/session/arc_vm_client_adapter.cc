// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <time.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/file_system_status.h"
#include "components/version_info/version_info.h"

namespace arc {
namespace {

constexpr const char kArcVmServerProxyJobName[] = "arcvm_2dserver_2dproxy";
constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";
constexpr const char kHomeDirectory[] = "/home";

constexpr int64_t kInvalidCid = -1;

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

// TODO(pliard): Export host-side /data to the VM, and remove the function.
vm_tools::concierge::CreateDiskImageRequest CreateArcDiskRequest(
    const std::string& user_id_hash,
    int64_t free_disk_bytes) {
  DCHECK(!user_id_hash.empty());

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(user_id_hash);
  request.set_disk_path("arcvm");

  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);

  // The logical size of the new disk image, in bytes.
  request.set_disk_size(free_disk_bytes / 2);

  return request;
}

std::string MonotonicTimestamp() {
  struct timespec ts;
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DPCHECK(ret == 0);
  const int64_t time =
      ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec;
  return base::NumberToString(time);
}

std::vector<std::string> GenerateKernelCmdline(
    const StartParams& start_params,
    const UpgradeParams& upgrade_params,
    const FileSystemStatus& file_system_status,
    bool is_dev_mode,
    bool is_host_on_vm,
    const std::string& channel,
    const std::string& serial_number) {
  DCHECK(!serial_number.empty());
  std::vector<std::string> result = {
      "androidboot.hardware=bertha",
      "androidboot.container=1",
      // TODO(b/139480143): when |start_params.native_bridge_experiment| is
      // enabled, switch to ndk_translation.
      "androidboot.native_bridge=libhoudini.so",
      base::StringPrintf("androidboot.dev_mode=%d", is_dev_mode),
      base::StringPrintf("androidboot.disable_runas=%d", !is_dev_mode),
      base::StringPrintf("androidboot.vm=%d", is_host_on_vm),
      base::StringPrintf("androidboot.debuggable=%d",
                         file_system_status.is_android_debuggable()),
      base::StringPrintf("androidboot.lcd_density=%d",
                         start_params.lcd_density),
      base::StringPrintf("androidboot.arc_file_picker=%d",
                         start_params.arc_file_picker_experiment),
      base::StringPrintf("androidboot.arc_custom_tabs=%d",
                         start_params.arc_custom_tabs_experiment),
      base::StringPrintf("androidboot.arc_print_spooler=%d",
                         start_params.arc_print_spooler_experiment),
      "androidboot.chromeos_channel=" + channel,
      "androidboot.boottime_offset=" + MonotonicTimestamp(),
      // TODO(yusukes): remove this once arcvm supports SELinux.
      "androidboot.selinux=permissive",

      // Since we don't do mini VM yet, set not only |start_params| but also
      // |upgrade_params| here for now.
      base::StringPrintf("androidboot.disable_boot_completed=%d",
                         upgrade_params.skip_boot_completed_broadcast),
      base::StringPrintf("androidboot.copy_packages_cache=%d",
                         static_cast<int>(upgrade_params.packages_cache_mode)),
      base::StringPrintf("androidboot.skip_gms_core_cache=%d",
                         upgrade_params.skip_gms_core_cache),
      base::StringPrintf("androidboot.arc_demo_mode=%d",
                         upgrade_params.is_demo_session),
      base::StringPrintf(
          "androidboot.supervision.transition=%d",
          static_cast<int>(upgrade_params.supervision_transition)),
      "androidboot.serialno=" + serial_number,
  };
  // TODO(yusukes): Check if we need to set ro.boot.container_boot_type and
  // ro.boot.enable_adb_sideloading for ARCVM.

  // Conditionally sets some properties based on |start_params|.
  switch (start_params.play_store_auto_update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      result.push_back("androidboot.play_store_auto_update=1");
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      result.push_back("androidboot.play_store_auto_update=0");
      break;
  }

  // Conditionally sets more properties based on |upgrade_params|.
  if (!upgrade_params.locale.empty()) {
    result.push_back("androidboot.locale=" + upgrade_params.locale);
    if (!upgrade_params.preferred_languages.empty()) {
      result.push_back(
          "androidboot.preferred_languages=" +
          base::JoinString(upgrade_params.preferred_languages, ","));
    }
  }

  // TODO(yusukes): Handle |demo_session_apps_path| in |upgrade_params|.
  // TODO(yusukes): Handle |is_managed_account| in |upgrade_params|.
  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    uint32_t cpus,
    const base::FilePath& data_disk_path,
    const FileSystemStatus& file_system_status,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);

  request.add_params("root=/dev/vda");
  if (file_system_status.is_host_rootfs_writable() &&
      file_system_status.is_system_image_ext_format()) {
    request.add_params("rw");
  }
  request.add_params("init=/init");
  for (auto& entry : kernel_cmdline)
    request.add_params(std::move(entry));

  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();

  vm->set_kernel(file_system_status.guest_kernel_path().value());

  // Add / as /dev/vda.
  vm->set_rootfs(file_system_status.system_image_path().value());
  request.set_rootfs_writable(file_system_status.is_host_rootfs_writable() &&
                              file_system_status.is_system_image_ext_format());

  // Add /data as /dev/vdb.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(data_disk_path.value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(true);
  // Add /vendor as /dev/vdc.
  disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());

  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add Android fstab.
  request.set_fstab(file_system_status.fstab_path().value());

  // Add cpus.
  request.set_cpus(cpus);

  return request;
}

// Gets a system property managed by crossystem. This function can be called
// only with base::MayBlock().
int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output))
    return -1;
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter,
                           public chromeos::ConciergeClient::VmObserver,
                           public chromeos::ConciergeClient::Observer {
 public:
  // Initializing |is_host_on_vm_| and |is_dev_mode_| is not always very fast.
  // Try to initialize them in the constructor and in StartMiniArc respectively.
  // They usually run when the system is not busy.
  explicit ArcVmClientAdapter(version_info::Channel channel)
      : ArcVmClientAdapter(channel, {}) {}

  // For testing purposes and the internal use (by the other ctor) only.
  ArcVmClientAdapter(version_info::Channel channel,
                     const FileSystemStatusRewriter& rewriter)
      : channel_(channel),
        is_host_on_vm_(chromeos::system::StatisticsProvider::GetInstance()
                           ->IsRunningOnVm()),
        file_system_status_rewriter_for_testing_(rewriter) {
    auto* client = GetConciergeClient();
    client->AddVmObserver(this);
    client->AddObserver(this);
  }

  ~ArcVmClientAdapter() override {
    auto* client = GetConciergeClient();
    client->RemoveObserver(this);
    client->RemoveVmObserver(this);
  }

  // chromeos::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(1) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    const int64_t cid = signal.cid();
    if (cid != current_cid_) {
      VLOG(1) << "Ignoring VmStopped signal: current CID=" << current_cid_
              << ", stopped CID=" << cid;
      return;
    }
    VLOG(1) << "OnVmStopped: ARCVM cid=" << cid;
    current_cid_ = kInvalidCid;
    OnArcInstanceStopped();
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(yusukes): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";

    // Save the parameters for the later call to UpgradeArc.
    start_params_ = std::move(params);

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            []() { return GetSystemPropertyInt("cros_debug") == 1; }),
        base::BindOnce(&ArcVmClientAdapter::OnIsDevMode,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(1) << "Starting Concierge service";
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
        base::BindOnce(&ArcVmClientAdapter::OnConciergeStarted,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void StopArcInstance(bool on_shutdown) override {
    if (on_shutdown) {
      // Do nothing when |on_shutdown| is true because either vm_concierge.conf
      // job (in case of user session termination) or session_manager (in case
      // of browser-initiated exit on e.g. chrome://flags or UI language change)
      // will stop all VMs including ARCVM right after the browser exits.
      VLOG(1)
          << "StopArcInstance is called during browser shutdown. Do nothing.";
      return;
    }

    VLOG(1) << "Stopping arcvm";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void SetUserInfo(const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(user_id_hash_.empty());
    DCHECK(serial_number_.empty());
    if (hash.empty())
      LOG(WARNING) << "hash is empty";
    if (serial_number.empty())
      LOG(WARNING) << "serial_number is empty";
    user_id_hash_ = hash;
    serial_number_ = serial_number;
  }

  // chromeos::ConciergeClient::Observer overrides:
  void ConciergeServiceStopped() override {
    VLOG(1) << "vm_concierge stopped";
    // At this point, all crosvm processes are gone. Notify the observer of the
    // event.
    OnArcInstanceStopped();
  }

  void ConciergeServiceRestarted() override {}

 private:
  void OnIsDevMode(chromeos::VoidDBusMethodCallback callback,
                   bool is_dev_mode) {
    // Make sure to kill a stale arcvm-server-proxy job (if any).
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStopped,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    is_dev_mode_ = is_dev_mode;
  }

  void OnArcVmServerProxyJobStopped(chromeos::VoidDBusMethodCallback callback,
                                    bool result) {
    VLOG(1) << "OnArcVmServerProxyJobStopped: job "
            << (result ? "stopped" : "not running?");

    should_notify_observers_ = true;
    // Always run the |callback| with true ignoring the |result|. |result| can
    // be false when the proxy job has already been stopped for other reasons,
    // but it's not considered as an error.
    std::move(callback).Run(true);
  }

  void OnConciergeStarted(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback,
                          bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to start Concierge service for arcvm";
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Starting arcvm-server-proxy";
    chromeos::UpstartClient::Get()->StartJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStarted,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnArcVmServerProxyJobStarted(UpgradeParams params,
                                    chromeos::VoidDBusMethodCallback callback,
                                    bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arcvm-server-proxy job";
      std::move(callback).Run(false);
      return;
    }
    // TODO(pliard): Export host-side /data to the VM, and remove the call. Note
    // that ArcSessionImpl checks low disk conditions before calling UpgradeArc.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                       base::FilePath(kHomeDirectory)),
        base::BindOnce(&ArcVmClientAdapter::CreateDiskImageAfterSizeCheck,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void CreateDiskImageAfterSizeCheck(UpgradeParams params,
                                     chromeos::VoidDBusMethodCallback callback,
                                     int64_t free_disk_bytes) {
    VLOG(2) << "Got free disk size: " << free_disk_bytes;
    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      std::move(callback).Run(false);
      return;
    }
    // TODO(pliard): Export host-side /data to the VM, and remove the call.
    GetConciergeClient()->CreateDiskImage(
        CreateArcDiskRequest(user_id_hash_, free_disk_bytes),
        base::BindOnce(&ArcVmClientAdapter::OnDiskImageCreated,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  // TODO(pliard): Export host-side /data to the VM, and remove the first half
  // of the function.
  void OnDiskImageCreated(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
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
    VLOG(1) << "Disk image for arcvm ready. status=" << response.status()
            << ", disk=" << response.disk_path();

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback),
                       base::FilePath(response.disk_path())));
  }

  void OnFileSystemStatus(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback,
                          const base::FilePath& data_disk_path,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    if (file_system_status_rewriter_for_testing_)
      file_system_status_rewriter_for_testing_.Run(&file_system_status);

    if (!file_system_status.property_files_expanded()) {
      LOG(ERROR) << "Failed to expand property files";
      std::move(callback).Run(false);
      return;
    }

    if (serial_number_.empty()) {
      LOG(ERROR) << "Serial number is not set";
      std::move(callback).Run(false);
      return;
    }

    const int32_t cpus =
        base::SysInfo::NumberOfProcessors() - start_params_.num_cores_disabled;
    DCHECK_LT(0, cpus);

    DCHECK(is_dev_mode_);
    std::vector<std::string> kernel_cmdline = GenerateKernelCmdline(
        start_params_, params, file_system_status, *is_dev_mode_,
        is_host_on_vm_, version_info::GetChannelString(channel_),
        serial_number_);
    auto start_request =
        CreateStartArcVmRequest(user_id_hash_, cpus, data_disk_path,
                                file_system_status, std::move(kernel_cmdline));

    GetConciergeClient()->StartArcVm(
        start_request,
        base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnStartArcVmReply(
      chromeos::VoidDBusMethodCallback callback,
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
    current_cid_ = response.vm_info().cid();

    VLOG(1) << "ARCVM started cid=" << current_cid_;
    std::move(callback).Run(true);
  }

  void OnArcInstanceStopped() {
    VLOG(1) << "ARCVM stopped. Stopping arcvm-server-proxy";

    // TODO(yusukes): Consider removing this stop call once b/142140355 is
    // implemented.
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        chromeos::EmptyVoidDBusMethodCallback());

    // If this method is called before even mini VM is started (e.g. very early
    // vm_concierge crash), or this method is called twice (e.g. crosvm crash
    // followed by vm_concierge crash), do nothing.
    if (!should_notify_observers_)
      return;
    should_notify_observers_ = false;

    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void OnStopVmReply(
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // If the reply indicates the D-Bus call is successfully done, do nothing.
    // Concierge will call OnVmStopped() eventually.
    if (reply.has_value() && reply.value().success())
      return;

    // We likely tried to stop mini VM which doesn't exist today. Notify
    // observers.
    // TODO(yusukes): Remove the fallback once we implement mini VM.
    OnArcInstanceStopped();
  }

  const version_info::Channel channel_;

  base::Optional<bool> is_dev_mode_;
  // True when the *host* is running on a VM.
  const bool is_host_on_vm_;

  // A hash of the primary profile user ID.
  std::string user_id_hash_;
  // A serial number for the current profile.
  std::string serial_number_;

  StartParams start_params_;
  bool should_notify_observers_ = false;
  int64_t current_cid_ = kInvalidCid;

  FileSystemStatusRewriter file_system_status_rewriter_for_testing_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter(
    version_info::Channel channel) {
  return std::make_unique<ArcVmClientAdapter>(channel);
}

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    version_info::Channel channel,
    const FileSystemStatusRewriter& rewriter) {
  return std::make_unique<ArcVmClientAdapter>(channel, rewriter);
}

}  // namespace arc
