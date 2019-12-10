// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "services/service_manager/sandbox/export.h"

namespace service_manager {

// Defines the sandbox types known within the servicemanager.
enum class SandboxType {
  // Not a valid sandbox type.
  kInvalid = -1,

  // Do not apply any sandboxing to the process.
  kNoSandbox,

#if defined(OS_WIN)
  // Do not apply any sandboxing and elevate the privileges of the process.
  kNoSandboxAndElevatedPrivileges,

  // The XR Compositing process.
  kXrCompositing,
#endif

#if defined(OS_FUCHSIA)
  // Sandbox type for the web::Context process on Fuchsia. Functionally it's an
  // equivalent of the browser process on other platforms.
  kWebContext,
#endif

  // Renderer or worker process. Most common case.
  kRenderer,

  // Utility process is as restrictive as the worker process except full
  // access is allowed to one configurable directory.
  kUtility,

  // GPU process.
  kGpu,

  // The PPAPI plugin process.
  kPpapi,

  // The network service process.
  kNetwork,

  // The CDM service process.
  kCdm,

#if defined(OS_MACOSX)
  // The NaCl loader process.
  kNaClLoader,
#endif  // defined(OS_MACOSX)

  // The pdf compositor service process.
  kPdfCompositor,

  // The profiling service process.
  kProfiling,

  // The audio service process.
  kAudio,

#if defined(OS_CHROMEOS)
  kIme,
#endif  // defined(OS_CHROMEOS)
};

SERVICE_MANAGER_SANDBOX_EXPORT bool IsUnsandboxedSandboxType(
    SandboxType sandbox_type);

SERVICE_MANAGER_SANDBOX_EXPORT void SetCommandLineFlagsForSandboxType(
    base::CommandLine* command_line,
    SandboxType sandbox_type);

SERVICE_MANAGER_SANDBOX_EXPORT SandboxType
SandboxTypeFromCommandLine(const base::CommandLine& command_line);

SERVICE_MANAGER_SANDBOX_EXPORT std::string StringFromUtilitySandboxType(
    SandboxType sandbox_type);

SERVICE_MANAGER_SANDBOX_EXPORT SandboxType
UtilitySandboxTypeFromString(const std::string& sandbox_string);

SERVICE_MANAGER_SANDBOX_EXPORT void EnableAudioSandbox(bool enable);

SERVICE_MANAGER_SANDBOX_EXPORT bool IsAudioSandboxEnabled();

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_
