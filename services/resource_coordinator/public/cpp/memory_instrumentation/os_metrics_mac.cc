// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <sys/param.h>

#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"

namespace memory_instrumentation {

namespace {

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
// The |phys_footprint| field was introduced in 10.11.
struct ChromeTaskVMInfo {
  mach_vm_size_t virtual_size;
  integer_t region_count;
  integer_t page_size;
  mach_vm_size_t resident_size;
  mach_vm_size_t resident_size_peak;
  mach_vm_size_t device;
  mach_vm_size_t device_peak;
  mach_vm_size_t internal;
  mach_vm_size_t internal_peak;
  mach_vm_size_t external;
  mach_vm_size_t external_peak;
  mach_vm_size_t reusable;
  mach_vm_size_t reusable_peak;
  mach_vm_size_t purgeable_volatile_pmap;
  mach_vm_size_t purgeable_volatile_resident;
  mach_vm_size_t purgeable_volatile_virtual;
  mach_vm_size_t compressed;
  mach_vm_size_t compressed_peak;
  mach_vm_size_t compressed_lifetime;
  mach_vm_size_t phys_footprint;
};
#else
using ChromeTaskVMInfo = task_vm_info;
#endif  // MAC_OS_X_VERSION_10_11
mach_msg_type_number_t ChromeTaskVMInfoCount =
    sizeof(ChromeTaskVMInfo) / sizeof(natural_t);

using VMRegion = mojom::VmRegion;

bool IsAddressInSharedRegion(uint64_t address) {
  return address >= SHARED_REGION_BASE_X86_64 &&
         address < (SHARED_REGION_BASE_X86_64 + SHARED_REGION_SIZE_X86_64);
}

bool IsRegionContainedInRegion(const VMRegion& containee,
                               const VMRegion& container) {
  uint64_t containee_end_address =
      containee.start_address + containee.size_in_bytes;
  uint64_t container_end_address =
      container.start_address + container.size_in_bytes;
  return containee.start_address >= container.start_address &&
         containee_end_address <= container_end_address;
}

bool DoRegionsIntersect(const VMRegion& a, const VMRegion& b) {
  uint64_t a_end_address = a.start_address + a.size_in_bytes;
  uint64_t b_end_address = b.start_address + b.size_in_bytes;
  return a.start_address < b_end_address && b.start_address < a_end_address;
}

// Creates VMRegions for all dyld images. Returns whether the operation
// succeeded.
bool GetDyldRegions(std::vector<VMRegion>* regions) {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr =
      task_info(mach_task_self(), TASK_DYLD_INFO,
                reinterpret_cast<task_info_t>(&dyld_info), &count);
  if (kr != KERN_SUCCESS)
    return false;

  const struct dyld_all_image_infos* all_image_infos =
      reinterpret_cast<const struct dyld_all_image_infos*>(
          dyld_info.all_image_info_addr);

  bool emitted_linkedit_from_dyld_shared_cache = false;
  for (size_t i = 0; i < all_image_infos->infoArrayCount; i++) {
    const char* image_name = all_image_infos->infoArray[i].imageFilePath;

    // The public definition for dyld_all_image_infos/dyld_image_info is wrong
    // for 64-bit platforms. We explicitly cast to struct mach_header_64 even
    // though the public definition claims that this is a struct mach_header.
    const struct mach_header_64* const header =
        reinterpret_cast<const struct mach_header_64* const>(
            all_image_infos->infoArray[i].imageLoadAddress);

    uint64_t next_command = reinterpret_cast<uint64_t>(header + 1);
    uint64_t command_end = next_command + header->sizeofcmds;
    uint64_t slide = 0;

    std::vector<VMRegion> temp_regions;
    std::string debug_id;
    for (unsigned int j = 0; j < header->ncmds; ++j) {
      // Ensure that next_command doesn't run past header->sizeofcmds.
      if (next_command + sizeof(struct load_command) > command_end)
        return false;
      const struct load_command* load_cmd =
          reinterpret_cast<const struct load_command*>(next_command);
      next_command += load_cmd->cmdsize;

      if (load_cmd->cmd == LC_SEGMENT_64) {
        if (load_cmd->cmdsize < sizeof(segment_command_64))
          return false;
        const segment_command_64* seg =
            reinterpret_cast<const segment_command_64*>(load_cmd);
        if (strcmp(seg->segname, SEG_PAGEZERO) == 0)
          continue;
        if (strcmp(seg->segname, SEG_TEXT) == 0) {
          slide = reinterpret_cast<uint64_t>(header) - seg->vmaddr;
        }

        // Avoid emitting LINKEDIT regions in the dyld shared cache, since they
        // all overlap.
        if (IsAddressInSharedRegion(seg->vmaddr) &&
            strcmp(seg->segname, SEG_LINKEDIT) == 0) {
          if (emitted_linkedit_from_dyld_shared_cache) {
            continue;
          } else {
            emitted_linkedit_from_dyld_shared_cache = true;
            image_name = "dyld shared cache combined __LINKEDIT";
          }
        }

        uint32_t protection_flags = 0;
        if (seg->initprot & VM_PROT_READ)
          protection_flags |= VMRegion::kProtectionFlagsRead;
        if (seg->initprot & VM_PROT_WRITE)
          protection_flags |= VMRegion::kProtectionFlagsWrite;
        if (seg->initprot & VM_PROT_EXECUTE)
          protection_flags |= VMRegion::kProtectionFlagsExec;

        VMRegion region;
        region.size_in_bytes = seg->vmsize;
        region.protection_flags = protection_flags;
        region.mapped_file = image_name;
        region.start_address = slide + seg->vmaddr;
        temp_regions.push_back(std::move(region));
      }

      if (load_cmd->cmd == LC_UUID) {
        if (load_cmd->cmdsize < sizeof(uuid_command))
          return false;
        const uuid_command* uuid_cmd =
            reinterpret_cast<const uuid_command*>(load_cmd);
        // The ID is comprised of the UUID concatenated with the module's "age"
        // value which is always 0.
        debug_id =
            base::HexEncode(&uuid_cmd->uuid, sizeof(uuid_cmd->uuid)) + "0";
      }
    }

    for (VMRegion& region : temp_regions) {
      region.module_debugid = debug_id;
      regions->push_back(region);
    }
  }
  return true;
}

// Creates VMRegions using mach vm syscalls. Returns whether the operation
// succeeded.
bool GetAllRegions(std::vector<VMRegion>* regions) {
  const int pid = getpid();
  task_t task = mach_task_self();
  mach_vm_size_t size = 0;
  mach_vm_address_t address = MACH_VM_MIN_ADDRESS;
  while (true) {
    base::CheckedNumeric<mach_vm_address_t> next_address(address);
    next_address += size;
    if (!next_address.IsValid())
      return false;
    address = next_address.ValueOrDie();

    vm_region_basic_info_64 basic_info;
    base::MachVMRegionResult result =
        base::GetBasicInfo(task, &size, &address, &basic_info);
    if (result == base::MachVMRegionResult::Error)
      return false;
    if (result == base::MachVMRegionResult::Finished)
      break;

    VMRegion region;

    if (basic_info.protection & VM_PROT_READ)
      region.protection_flags |= VMRegion::kProtectionFlagsRead;
    if (basic_info.protection & VM_PROT_WRITE)
      region.protection_flags |= VMRegion::kProtectionFlagsWrite;
    if (basic_info.protection & VM_PROT_EXECUTE)
      region.protection_flags |= VMRegion::kProtectionFlagsExec;

    char buffer[MAXPATHLEN];
    int length = proc_regionfilename(pid, address, buffer, MAXPATHLEN);
    if (length != 0)
      region.mapped_file.assign(buffer, length);

    // There's no way to get swapped or clean bytes without doing a
    // very expensive syscalls that crawls every single page in the memory
    // object.
    region.start_address = address;
    region.size_in_bytes = size;
    regions->push_back(region);
  }
  return true;
}

void AddRegionByteStats(VMRegion* dest, const VMRegion& source) {
  dest->byte_stats_private_dirty_resident +=
      source.byte_stats_private_dirty_resident;
  dest->byte_stats_private_clean_resident +=
      source.byte_stats_private_clean_resident;
  dest->byte_stats_shared_dirty_resident +=
      source.byte_stats_shared_dirty_resident;
  dest->byte_stats_shared_clean_resident +=
      source.byte_stats_shared_clean_resident;
  dest->byte_stats_swapped += source.byte_stats_swapped;
  dest->byte_stats_proportional_resident +=
      source.byte_stats_proportional_resident;
}

}  // namespace

#include <sys/sysctl.h>
bool GetCPUTypeForProcess(pid_t pid, cpu_type_t* cpu_type) {
  size_t len = sizeof(*cpu_type);
  int result = sysctlbyname("sysctl.proc_cputype",
                            cpu_type,
                            &len,
                            NULL,
                            0);
  if (result != 0) {
    DPLOG(ERROR) << "sysctlbyname(""sysctl.proc_cputype"")";
    return false;
  }

  return true;
}
bool IsAddressInSharedRegion(mach_vm_address_t addr, cpu_type_t type) {
  if (type == CPU_TYPE_I386) {
    return addr >= SHARED_REGION_BASE_I386 &&
           addr < (SHARED_REGION_BASE_I386 + SHARED_REGION_SIZE_I386);
  } else if (type == CPU_TYPE_X86_64) {
    return addr >= SHARED_REGION_BASE_X86_64 &&
           addr < (SHARED_REGION_BASE_X86_64 + SHARED_REGION_SIZE_X86_64);
  } else {
    return false;
  }
}

bool GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  size_t private_pages_count = 0;
  size_t shared_pages_count = 0;
  mach_port_t process_ = mach_task_self();
  if (!private_bytes && !shared_bytes)
    return true;

  mach_port_t task = mach_task_self();
  if (task == MACH_PORT_NULL) {
    DLOG(ERROR) << "Invalid process";
    return false;
  }

  cpu_type_t cpu_type;
  if (!GetCPUTypeForProcess(process_, &cpu_type))
    return false;

  // The same region can be referenced multiple times. To avoid double counting
  // we need to keep track of which regions we've already counted.
  std::set<int> seen_objects;

  // We iterate through each VM region in the task's address map. For shared
  // memory we add up all the pages that are marked as shared. Like libtop we
  // try to avoid counting pages that are also referenced by other tasks. Since
  // we don't have access to the VM regions of other tasks the only hint we have
  // is if the address is in the shared region area.
  //
  // Private memory is much simpler. We simply count the pages that are marked
  // as private or copy on write (COW).
  //
  // See libtop_update_vm_regions in
  // http://www.opensource.apple.com/source/top/top-67/libtop.c
  mach_vm_size_t size = 0;
  for (mach_vm_address_t address = MACH_VM_MIN_ADDRESS;; address += size) {
    vm_region_top_info_data_t info;
    mach_msg_type_number_t info_count = VM_REGION_TOP_INFO_COUNT;
    mach_port_t object_name;
    kern_return_t kr = mach_vm_region(task,
                                      &address,
                                      &size,
                                      VM_REGION_TOP_INFO,
                                      reinterpret_cast<vm_region_info_t>(&info),
                                      &info_count,
                                      &object_name);
    if (kr == KERN_INVALID_ADDRESS) {
      // We're at the end of the address space.
      break;
    } else if (kr != KERN_SUCCESS) {
//      MACH_DLOG(ERROR, kr) << "mach_vm_region";
      return false;
    }

    // The kernel always returns a null object for VM_REGION_TOP_INFO, but
    // balance it with a deallocate in case this ever changes. See 10.9.2
    // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
    mach_port_deallocate(mach_task_self(), object_name);

    if (IsAddressInSharedRegion(address, cpu_type) &&
        info.share_mode != SM_PRIVATE)
      continue;

    if (info.share_mode == SM_COW && info.ref_count == 1)
      info.share_mode = SM_PRIVATE;

    switch (info.share_mode) {
      case SM_PRIVATE:
        private_pages_count += info.private_pages_resident;
        private_pages_count += info.shared_pages_resident;
        break;
      case SM_COW:
        private_pages_count += info.private_pages_resident;
        // Fall through
      case SM_SHARED:
        if (seen_objects.count(info.obj_id) == 0) {
          // Only count the first reference to this region.
          seen_objects.insert(info.obj_id);
          shared_pages_count += info.shared_pages_resident;
        }
        break;
      default:
        break;
    }
  }

  if (private_bytes)
    *private_bytes = private_pages_count * PAGE_SIZE;
  if (shared_bytes)
    *shared_bytes = shared_pages_count * PAGE_SIZE;

  return true;
}

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  if(__builtin_available(macOS 10.9,*)) {
  ChromeTaskVMInfo task_vm_info;
  mach_msg_type_number_t count = ChromeTaskVMInfoCount;
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_vm_info), &count);
  if (result != KERN_SUCCESS)
    return false;

  dump->platform_private_footprint->internal_bytes = task_vm_info.internal;
  dump->platform_private_footprint->compressed_bytes = task_vm_info.compressed;


  if (count == ChromeTaskVMInfoCount) 
    dump->platform_private_footprint->phys_footprint_bytes =
        task_vm_info.phys_footprint;
  } else {
    size_t private_size, shared_size;
    mach_msg_type_number_t count = 10;
    struct task_basic_info_64 task_info_data;
    kern_return_t kr = task_info(mach_task_self(),
                                 TASK_BASIC_INFO_64,
                                 reinterpret_cast<task_info_t>(&task_info_data),
                                 &count);
    if(kr == KERN_SUCCESS) {
      dump->platform_private_footprint->internal_bytes = task_info_data.resident_size;
      dump->platform_private_footprint->phys_footprint_bytes = task_info_data.resident_size;
	}
    
	if(GetMemoryBytes(&private_size,&shared_size)) {
      dump->platform_private_footprint->private_bytes = private_size;
	} else {
	  return false;
	}
  }
  return true;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessId pid) {
  std::vector<mojom::VmRegionPtr> maps;

  std::vector<VMRegion> dyld_regions;
  if (!GetDyldRegions(&dyld_regions))
    return maps;
  std::vector<VMRegion> all_regions;
  if (!GetAllRegions(&all_regions))
    return maps;

  // Merge information from dyld regions and all regions.
  for (const VMRegion& region : all_regions) {
    bool skip = false;
    const bool in_shared_region = IsAddressInSharedRegion(region.start_address);
    for (VMRegion& dyld_region : dyld_regions) {
      // If this region is fully contained in a dyld region, then add the bytes
      // stats.
      if (IsRegionContainedInRegion(region, dyld_region)) {
        AddRegionByteStats(&dyld_region, region);
        skip = true;
        break;
      }

      // Check to see if the region is likely used for the dyld shared cache.
      if (in_shared_region) {
        // This region is likely used for the dyld shared cache. Don't record
        // any byte stats since:
        //   1. It's not possible to figure out which dyld regions the byte
        //      stats correspond to.
        //   2. The region is likely shared by non-Chrome processes, so there's
        //      no point in charging the pages towards Chrome.
        if (DoRegionsIntersect(region, dyld_region)) {
          skip = true;
          break;
        }
      }
    }
    if (skip)
      continue;

    maps.push_back(VMRegion::New(region));
  }

  for (VMRegion& region : dyld_regions) {
    maps.push_back(VMRegion::New(region));
  }

  return maps;
}

std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessModules(
    base::ProcessId pid) {
  std::vector<mojom::VmRegionPtr> maps;

  std::vector<VMRegion> dyld_regions;
  if (!GetDyldRegions(&dyld_regions))
    return maps;

  for (VMRegion& region : dyld_regions) {
    maps.push_back(VMRegion::New(region));
  }

  return maps;
}

}  // namespace memory_instrumentation
