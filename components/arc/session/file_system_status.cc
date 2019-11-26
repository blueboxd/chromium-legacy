// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/file_system_status.h"

#include <linux/magic.h>
#include <sys/statvfs.h>

#include <string>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/arc/session/arc_vm_client_adapter_util.h"

namespace arc {
namespace {

constexpr const char kArcVmConfigJsonPath[] = "/usr/share/arcvm/config.json";
constexpr const char kBuiltinPath[] = "/opt/google/vms/android";
constexpr const char kDlcPath[] = "/run/imageloader/arcvm-dlc/package/root";
constexpr const char kFstab[] = "fstab";
constexpr const char kGeneratedPropertyFilesPath[] =
    "/run/arcvm/host_generated";
constexpr const char kKernel[] = "vmlinux";
constexpr const char kPropertyFilesPath[] = "/usr/share/arcvm/properties";
constexpr const char kRootFs[] = "system.raw.img";
constexpr const char kVendorImage[] = "vendor.raw.img";

}  // namespace

FileSystemStatus::FileSystemStatus(FileSystemStatus&& other) = default;
FileSystemStatus::~FileSystemStatus() = default;
FileSystemStatus& FileSystemStatus::operator=(FileSystemStatus&& other) =
    default;

FileSystemStatus::FileSystemStatus()
    : is_android_debuggable_(
          IsAndroidDebuggable(base::FilePath(kArcVmConfigJsonPath))),
      is_host_rootfs_writable_(IsHostRootfsWritable()),
      system_image_path_(SelectDlcOrBuiltin(base::FilePath(kRootFs))),
      vendor_image_path_(SelectDlcOrBuiltin(base::FilePath(kVendorImage))),
      guest_kernel_path_(SelectDlcOrBuiltin(base::FilePath(kKernel))),
      fstab_path_(SelectDlcOrBuiltin(base::FilePath(kFstab))),
      property_files_expanded_(
          ExpandPropertyFiles(base::FilePath(kPropertyFilesPath),
                              base::FilePath(kGeneratedPropertyFilesPath))),
      is_system_image_ext_format_(IsSystemImageExtFormat(system_image_path_)) {}

// static
bool FileSystemStatus::IsAndroidDebuggable(const base::FilePath& json_path) {
  if (!base::PathExists(json_path))
    return false;

  std::string content;
  if (!base::ReadFileToString(json_path, &content))
    return false;

  base::JSONReader::ValueWithError result(
      base::JSONReader::ReadAndReturnValueWithError(content,
                                                    base::JSON_PARSE_RFC));
  if (!result.value) {
    LOG(ERROR) << "Error parsing " << json_path
               << ": code=" << result.error_code
               << ", message=" << result.error_message << ": " << content;
    return false;
  }
  if (!result.value->is_dict()) {
    LOG(ERROR) << "Error parsing " << json_path << ": " << *(result.value);
    return false;
  }

  const base::Value* debuggable = result.value->FindKeyOfType(
      "ANDROID_DEBUGGABLE", base::Value::Type::BOOLEAN);
  if (!debuggable) {
    LOG(ERROR) << "ANDROID_DEBUGGABLE is not found in " << json_path;
    return false;
  }

  return debuggable->GetBool();
}

// static
bool FileSystemStatus::IsHostRootfsWritable() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  struct statvfs buf;
  if (statvfs("/", &buf) < 0) {
    PLOG(ERROR) << "statvfs() failed";
    return false;
  }
  const bool rw = !(buf.f_flag & ST_RDONLY);
  VLOG(1) << "Host's rootfs is " << (rw ? "rw" : "ro");
  return rw;
}

// static
base::FilePath FileSystemStatus::SelectDlcOrBuiltin(
    const base::FilePath& file) {
  const base::FilePath dlc_path = base::FilePath(kDlcPath).Append(file);
  if (base::PathExists(dlc_path)) {
    VLOG(1) << "arcvm-dlc will be used for " << file.value();
    return dlc_path;
  }
  return base::FilePath(kBuiltinPath).Append(file);
}

// static
bool FileSystemStatus::ExpandPropertyFiles(const base::FilePath& source_path,
                                           const base::FilePath& dest_path) {
  CrosConfig config;
  for (const char* file : {"default.prop", "build.prop"}) {
    if (!ExpandPropertyFile(source_path.Append(file), dest_path.Append(file),
                            &config)) {
      LOG(ERROR) << "Failed to expand " << source_path.Append(file);
      return false;
    }
  }
  return true;
}

// static
bool FileSystemStatus::IsSystemImageExtFormat(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Cannot open system image file: " << path.value();
    return false;
  }

  uint8_t buf[2];
  if (!file.ReadAndCheck(0x400 + 0x38, base::make_span(buf, sizeof(buf)))) {
    PLOG(ERROR) << "File read error on system image file: " << path.value();
    return false;
  }

  uint16_t magic_le = *reinterpret_cast<uint16_t*>(buf);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return magic_le == EXT4_SUPER_MAGIC;
#else
#error Unsupported platform
#endif
}

}  // namespace arc
