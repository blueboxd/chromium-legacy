// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_disk.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/base/text/bytes_formatting.h"

namespace {
chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

std::string FormatBytes(const int64_t value) {
  return base::UTF16ToUTF8(ui::FormatBytes(value));
}
}  // namespace

namespace crostini {
CrostiniDiskInfo::CrostiniDiskInfo() = default;
CrostiniDiskInfo::CrostiniDiskInfo(CrostiniDiskInfo&&) = default;
CrostiniDiskInfo& CrostiniDiskInfo::operator=(CrostiniDiskInfo&&) = default;
CrostiniDiskInfo::~CrostiniDiskInfo() = default;

namespace disk {

void GetDiskInfo(OnceDiskInfoCallback callback,
                 Profile* profile,
                 std::string vm_name) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&OnAmountOfFreeDiskSpace, std::move(callback), profile,
                     std::move(vm_name)));
}

void OnAmountOfFreeDiskSpace(OnceDiskInfoCallback callback,
                             Profile* profile,
                             std::string vm_name,
                             int64_t free_space) {
  if (free_space == 0) {
    LOG(ERROR) << "Failed to get amount of free disk space";
    std::move(callback).Run(nullptr);
  } else {
    auto container_id = ContainerId(vm_name, kCrostiniDefaultContainerName);
    CrostiniManager::GetForProfile(profile)->EnsureVmRunning(
        std::move(container_id),
        base::BindOnce(&OnVMRunning, std::move(callback), profile,
                       std::move(vm_name), free_space));
  }
}

void OnVMRunning(OnceDiskInfoCallback callback,
                 Profile* profile,
                 std::string vm_name,
                 int64_t free_space,
                 CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to launch VM: error " << static_cast<int>(result);
    std::move(callback).Run(nullptr);
  } else {
    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(CryptohomeIdForProfile(profile));
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    GetConciergeClient()->ListVmDisks(
        std::move(request), base::BindOnce(&OnListVmDisks, std::move(callback),
                                           std::move(vm_name), free_space));
  }
}

void OnListVmDisks(
    OnceDiskInfoCallback callback,
    std::string vm_name,
    int64_t free_space,
    base::Optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get response from concierge";
    std::move(callback).Run(nullptr);
    return;
  }
  if (!response->success()) {
    LOG(ERROR) << "Failed to get successful response from concierge "
               << response->failure_reason();
    std::move(callback).Run(nullptr);
    return;
  }
  auto disk_info = std::make_unique<CrostiniDiskInfo>();
  auto image =
      std::find_if(response->images().begin(), response->images().end(),
                   [&vm_name](const auto& a) { return a.name() == vm_name; });
  if (image == response->images().end()) {
    // No match found for the VM:
    LOG(ERROR) << "No VM found with name " << vm_name;
    std::move(callback).Run(nullptr);
    return;
  }
  if (image->image_type() !=
      vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW) {
    // Can't resize qcow2 images and don't know how to handle auto or pluginvm
    // images.
    disk_info->can_resize = false;
    std::move(callback).Run(std::move(disk_info));
    return;
  }
  disk_info->is_user_chosen_size = image->user_chosen_size();
  disk_info->can_resize =
      image->image_type() == vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW;

  std::vector<crostini::mojom::DiskSliderTickPtr> ticks =
      GetTicks(*image, image->min_size(), image->size(),
               free_space + image->size(), &(disk_info->default_index));
  if (ticks.size() == 0) {
    LOG(ERROR) << "Unable to calculate the number of ticks for "
               << image->min_size() << " " << image->size() << " "
               << free_space + image->size();
    std::move(callback).Run(nullptr);
    return;
  }
  disk_info->ticks = std::move(ticks);

  std::move(callback).Run(std::move(disk_info));
}

std::vector<crostini::mojom::DiskSliderTickPtr> GetTicks(
    const vm_tools::concierge::VmDiskInfo& info,
    int64_t min,
    int64_t current,
    int64_t max,
    int* out_default_index) {
  if (min > current || current > max) {
    return {};
  }
  std::vector<int64_t> values = GetTicksForDiskSize(min, max);

  // Find the first value which is >= the current size, round that value down to
  // the current size, and then record that as the default. This means that the
  // ticks won't be evenly spaced, but GetTicksForDiskSize uses ~100 ticks so
  // close enough.
  auto it = std::lower_bound(begin(values), end(values), current);
  if (it != end(values)) {
    *it = current;
    *out_default_index = std::distance(begin(values), it);
  }

  std::vector<crostini::mojom::DiskSliderTickPtr> ticks;
  ticks.reserve(values.size());
  for (const auto& val : values) {
    std::string formatted_val = FormatBytes(val);
    ticks.emplace_back(crostini::mojom::DiskSliderTick::New(val, formatted_val,
                                                            formatted_val));
  }
  return ticks;
}
}  // namespace disk
}  // namespace crostini
