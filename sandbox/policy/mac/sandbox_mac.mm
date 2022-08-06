// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include <CoreFoundation/CFTimeZone.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_port_rendezvous.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mac/audio.sb.h"
#include "sandbox/policy/mac/cdm.sb.h"
#include "sandbox/policy/mac/common.sb.h"
#include "sandbox/policy/mac/gpu.sb.h"
#include "sandbox/policy/mac/mirroring.sb.h"
#include "sandbox/policy/mac/nacl_loader.sb.h"
#include "sandbox/policy/mac/network.sb.h"
#include "sandbox/policy/mac/ppapi.sb.h"
#include "sandbox/policy/mac/print_backend.sb.h"
#include "sandbox/policy/mac/print_compositor.sb.h"
#include "sandbox/policy/mac/renderer.sb.h"
#include "sandbox/policy/mac/screen_ai.sb.h"
#include "sandbox/policy/mac/speech_recognition.sb.h"
#include "sandbox/policy/mac/utility.sb.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace sandbox {
namespace policy {

// Load the appropriate template for the given sandbox type.
// Returns the template as a string or an empty string on error.
std::string LoadSandboxTemplate(sandbox::mojom::Sandbox sandbox_type) {
  DCHECK_EQ(sandbox_type, sandbox::mojom::Sandbox::kGpu);
  return kSeatbeltPolicyString_gpu;
}

// Turns on the OS X sandbox for this process.

// static
bool SandboxMac::Enable(sandbox::mojom::Sandbox sandbox_type) {
  DCHECK_EQ(sandbox_type, sandbox::mojom::Sandbox::kGpu);

  std::string sandbox_data = LoadSandboxTemplate(sandbox_type);
  if (sandbox_data.empty())
    return false;

  SandboxCompiler compiler(sandbox_data);

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  std::string home_dir = [NSHomeDirectory() fileSystemRepresentation];
  base::FilePath home_dir_canonical =
      GetCanonicalPath(base::FilePath(home_dir));

  if (sandbox_type == sandbox::mojom::Sandbox::kGpu) {
    base::FilePath bundle_path =
        GetCanonicalPath(base::mac::FrameworkBundlePath());
  }

  // Initialize sandbox.
  std::string error_str;
  bool success = compiler.CompileAndApplyProfile(&error_str);
  DLOG_IF(FATAL, !success) << "Failed to initialize sandbox: " << error_str;
  return success;
}

base::FilePath GetCanonicalPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(ERROR) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

std::string GetSandboxProfile(sandbox::mojom::Sandbox sandbox_type) {
  std::string profile = std::string(kSeatbeltPolicyString_common);

  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kAudio:
      profile += kSeatbeltPolicyString_audio;
      break;
    case sandbox::mojom::Sandbox::kCdm:
      profile += kSeatbeltPolicyString_cdm;
      break;
    case sandbox::mojom::Sandbox::kGpu:
      profile += kSeatbeltPolicyString_gpu;
      break;
    case sandbox::mojom::Sandbox::kMirroring:
      profile += kSeatbeltPolicyString_mirroring;
      break;
    case sandbox::mojom::Sandbox::kNaClLoader:
      profile += kSeatbeltPolicyString_nacl_loader;
      break;
    case sandbox::mojom::Sandbox::kNetwork:
      profile += kSeatbeltPolicyString_network;
      break;
    case sandbox::mojom::Sandbox::kPpapi:
      profile += kSeatbeltPolicyString_ppapi;
      break;
#if BUILDFLAG(ENABLE_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      profile += kSeatbeltPolicyString_print_backend;
      break;
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
      profile += kSeatbeltPolicyString_print_compositor;
      break;
    case sandbox::mojom::Sandbox::kScreenAI:
      profile += kSeatbeltPolicyString_screen_ai;
      break;
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      profile += kSeatbeltPolicyString_speech_recognition;
      break;
    // kService and kUtility are the same on OS_MAC, so fallthrough.
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kUtility:
      profile += kSeatbeltPolicyString_utility;
      break;
    case sandbox::mojom::Sandbox::kRenderer:
      profile += kSeatbeltPolicyString_renderer;
      break;
    case sandbox::mojom::Sandbox::kNoSandbox:
      CHECK(false);
      break;
  }
  return profile;
}

}  // namespace policy
}  // namespace sandbox
