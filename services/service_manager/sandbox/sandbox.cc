// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/sandbox.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "services/service_manager/sandbox/switches.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX)
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "base/process/process_info.h"
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif  // defined(OS_WIN)

namespace service_manager {

#if defined(OS_LINUX)
bool Sandbox::Initialize(SandboxType sandbox_type,
                         SandboxLinux::PreSandboxHook hook,
                         const SandboxLinux::Options& options) {
  return SandboxLinux::GetInstance()->InitializeSandbox(
      sandbox_type, std::move(hook), options);
}
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
bool Sandbox::Initialize(SandboxType sandbox_type, base::OnceClosure hook) {
  // Warm up APIs before turning on the sandbox.
  SandboxMac::Warmup(sandbox_type);

  // Execute the post warmup callback.
  if (!hook.is_null())
    std::move(hook).Run();

  // Actually sandbox the process.
  return SandboxMac::Enable(sandbox_type);
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
bool Sandbox::Initialize(SandboxType sandbox_type,
                         sandbox::SandboxInterfaceInfo* sandbox_info) {
  sandbox::BrokerServices* broker_services = sandbox_info->broker_services;
  if (broker_services) {
    if (!SandboxWin::InitBrokerServices(broker_services))
      return false;

    // IMPORTANT: This piece of code needs to run as early as possible in the
    // process because it will initialize the sandbox broker, which requires the
    // process to swap its window station. During this time all the UI will be
    // broken. This has to run before threads and windows are created.
    if (!IsUnsandboxedSandboxType(sandbox_type)) {
      // Precreate the desktop and window station used by the renderers.
      scoped_refptr<sandbox::TargetPolicy> policy =
          broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(true);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
    }
    return true;
  }
  return IsUnsandboxedSandboxType(sandbox_type) ||
         SandboxWin::InitTargetServices(sandbox_info->target_services);
}
#endif  // defined(OS_WIN)

// static
bool Sandbox::IsProcessSandboxed() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool is_browser = !command_line->HasSwitch(switches::kProcessType);

  if (!is_browser &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    // When running with --no-sandbox, unconditionally report the process as
    // sandboxed. This lets code write |DCHECK(IsProcessSandboxed())| and not
    // break when testing with the --no-sandbox switch.
    return true;
  }

#if defined(OS_ANDROID)
  // Note that this does not check the status of the Seccomp sandbox. Call
  // https://developer.android.com/reference/android/os/Process#isIsolated().
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jclass> process_class =
      base::android::GetClass(env, "android/os/Process");
  jmethodID is_isolated =
      base::android::MethodID::Get<base::android::MethodID::TYPE_STATIC>(
          env, process_class.obj(), "isIsolated", "()Z");
  return env->CallStaticBooleanMethod(process_class.obj(), is_isolated);
#elif defined(OS_FUCHSIA)
  // TODO(https://crbug.com/1071420): Figure out what to do here. Process
  // launching controls the sandbox and there are no ambient capabilities, so
  // basically everything but the browser is considered sandboxed.
  return !is_browser;
#elif defined(OS_LINUX)
  int status = SandboxLinux::GetInstance()->GetStatus();
  constexpr int kLayer1Flags = SandboxLinux::Status::kSUID |
                               SandboxLinux::Status::kPIDNS |
                               SandboxLinux::Status::kUserNS;
  constexpr int kLayer2Flags =
      SandboxLinux::Status::kSeccompBPF | SandboxLinux::Status::kSeccompTSYNC;
  return (status & kLayer1Flags) != 0 && (status & kLayer2Flags) != 0;
#elif defined(OS_MACOSX)
  return sandbox::Seatbelt::IsSandboxed();
#elif defined(OS_WIN)
  return base::GetCurrentProcessIntegrityLevel() < base::MEDIUM_INTEGRITY;
#else
  return false;
#endif
}

}  // namespace service_manager
