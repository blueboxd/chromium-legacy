// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_dispatcher.h"

#include <stdint.h>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/signed_interception.h"
#include "sandbox/win/src/signed_policy.h"

namespace sandbox {

SignedDispatcher::SignedDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
      {IPC_NTCREATESECTION_TAG, {VOIDPTR_TYPE}},
      reinterpret_cast<CallbackGeneric>(&SignedDispatcher::CreateSection)};

  ipc_calls_.push_back(create_params);
}

bool SignedDispatcher::SetupService(InterceptionManager* manager, int service) {
  if (service == IPC_NTCREATESECTION_TAG)
    return INTERCEPT_NT(manager, NtCreateSection, CREATE_SECTION_ID, 32);
  return false;
}

bool SignedDispatcher::CreateSection(IPCInfo* ipc, HANDLE file_handle) {
  // Duplicate input handle from target to broker.
  HANDLE local_file_handle = nullptr;
  if (!::DuplicateHandle((*ipc->client_info).process, file_handle,
                         ::GetCurrentProcess(), &local_file_handle,
                         FILE_MAP_EXECUTE, false, 0)) {
    return false;
  }

  base::win::ScopedHandle local_handle(local_file_handle);
  base::string16 path;
  if (!GetPathFromHandle(local_handle.Get(), &path))
    return false;
  const wchar_t* module_name = path.c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(module_name);

  EvalResult result =
      policy_base_->EvalPolicy(IPC_NTCREATESECTION_TAG, params.GetBase());

  // Return operation status on the IPC.
  HANDLE section_handle = nullptr;
  ipc->return_info.nt_status = SignedPolicy::CreateSectionAction(
      result, *ipc->client_info, local_handle, &section_handle);
  ipc->return_info.handle = section_handle;
  return true;
}

}  // namespace sandbox
