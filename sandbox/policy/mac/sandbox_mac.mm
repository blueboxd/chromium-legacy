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
#include "base/ignore_result.h"
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
#include "sandbox/policy/mac/speech_recognition.sb.h"
#include "sandbox/policy/mac/utility.sb.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace sandbox {
namespace policy {

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0

// static
void SandboxMac::Warmup(sandbox::mojom::Sandbox sandbox_type) {
  DCHECK_EQ(sandbox_type, sandbox::mojom::Sandbox::kGpu);

  @autoreleasepool {
    {  // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
      base::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
          CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

      // Allocate a 1x1 image.
      char data[4];
      base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
          data, 1, 1, 8, 1 * 4, rgb_colorspace,
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

      // Load in the color profiles we'll need (as a side effect).
      base::IgnoreResult(base::mac::GetSRGBColorSpace());
      base::IgnoreResult(base::mac::GetSystemColorSpace());

      // CGColorSpaceCreateSystemDefaultCMYK - 10.6
      base::ScopedCFTypeRef<CGColorSpaceRef> cmyk_colorspace(
          CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
    }

    {  // localtime() - 10.5.6
      time_t tv = {0};
      localtime(&tv);
    }

    {  // Gestalt() tries to read
       // /System/Library/CoreServices/SystemVersion.plist
      // on 10.5.6
      int32_t tmp;
      base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
    }

    {  // CGImageSourceGetStatus() - 10.6
       // Create a png with just enough data to get everything warmed up...
      unsigned char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
      NSData* data = [NSData dataWithBytes:png_header
                                    length:base::size(png_header)];
      base::ScopedCFTypeRef<CGImageSourceRef> img(
          CGImageSourceCreateWithData((CFDataRef)data, NULL));
      CGImageSourceGetStatus(img);
    }

    {
      // Allow access to /dev/urandom.
      base::GetUrandomFD();
    }

    {  // IOSurfaceLookup() - 10.7
      // Needed by zero-copy texture update framework - crbug.com/323338
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceLookup(0));
    }
  }
}

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
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      profile += kSeatbeltPolicyString_speech_recognition;
      break;
    // kService and kUtility are the same on OS_MAC, so fallthrough.
    case sandbox::mojom::Sandbox::kService:
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
