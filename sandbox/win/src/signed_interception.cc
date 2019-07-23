// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_interception.h"

#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

NTSTATUS WINAPI
TargetNtCreateSection(NtCreateSectionFunction orig_CreateSection,
                      PHANDLE section_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      PLARGE_INTEGER maximum_size,
                      ULONG section_page_protection,
                      ULONG allocation_attributes,
                      HANDLE file_handle) {
  NTSTATUS status = orig_CreateSection(
      section_handle, desired_access, object_attributes, maximum_size,
      section_page_protection, allocation_attributes, file_handle);

  // Only intercept calls that match a particular signature.
  if (status != STATUS_INVALID_IMAGE_HASH)
    return status;
  if (desired_access != (SECTION_QUERY | SECTION_MAP_WRITE | SECTION_MAP_READ |
                         SECTION_MAP_EXECUTE))
    return status;
  if (object_attributes)
    return status;
  if (maximum_size)
    return status;
  if (section_page_protection != PAGE_EXECUTE)
    return status;
  if (allocation_attributes != SEC_IMAGE)
    return status;

  do {
    if (!ValidParameter(section_handle, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (!memory)
      break;
    std::unique_ptr<wchar_t, NtAllocDeleter> path;
    if (!NtGetPathFromHandle(file_handle, &path))
      break;
    const wchar_t* const_name = path.get();

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(const_name);

    if (!QueryBroker(IPC_NTCREATESECTION_TAG, params.GetBase()))
      break;

    CrossCallReturn answer = {0};
    answer.nt_status = status;
    SharedMemIPCClient ipc(memory);
    ResultCode code =
        CrossCall(ipc, IPC_NTCREATESECTION_TAG, file_handle, &answer);

    if (code != SBOX_ALL_OK)
      break;

    status = answer.nt_status;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *section_handle = answer.handle;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

}  // namespace sandbox
