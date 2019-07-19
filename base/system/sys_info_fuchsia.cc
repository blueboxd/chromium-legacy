// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <sys/utsname.h>
#include <zircon/syscalls.h>

#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {

// static
int64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return zx_system_get_physmem();
}

// static
int64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  // TODO(https://crbug.com/706592): This method doesn't have an Fuchsia API to
  // consume it
  NOTREACHED();
  return 0;
}

// static
int SysInfo::NumberOfProcessors() {
  return zx_system_get_num_cpus();
}

// static
int64_t SysInfo::AmountOfVirtualMemory() {
  return 0;
}

// static
std::string SysInfo::OperatingSystemName() {
  return "Fuchsia";
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  NOTIMPLEMENTED_LOG_ONCE();
  return -1;
}

// static
int64_t SysInfo::AmountOfTotalDiskSpace(const FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  NOTIMPLEMENTED_LOG_ONCE();
  return -1;
}

// static
std::string SysInfo::OperatingSystemVersion() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }

  return std::string(info.release);
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  struct utsname info;
  if (uname(&info) < 0) {
    *major_version = 0;
    *minor_version = 0;
    *bugfix_version = 0;
    return;
  }
  int num_read = sscanf(info.release, "%d.%d.%d", major_version, minor_version,
                        bugfix_version);
  if (num_read < 1)
    *major_version = 0;
  if (num_read < 2)
    *minor_version = 0;
  if (num_read < 3)
    *bugfix_version = 0;
}

// static
std::string SysInfo::OperatingSystemArchitecture() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }

  return info.machine;
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
