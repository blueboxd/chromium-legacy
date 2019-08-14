// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/pdf_child_init.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/win/current_module.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/sandbox/switches.h"
#endif

namespace {

#if defined(OS_WIN)
typedef decltype(::GetFontData)* GetFontDataPtr;
GetFontDataPtr g_original_get_font_data = nullptr;


DWORD WINAPI GetFontDataPatch(HDC hdc,
                              DWORD table,
                              DWORD offset,
                              LPVOID buffer,
                              DWORD length) {
  DWORD rv = g_original_get_font_data(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      if (content::ChildThread::Get())
        content::ChildThread::Get()->PreCacheFont(logfont);
      rv = g_original_get_font_data(hdc, table, offset, buffer, length);
      if (content::ChildThread::Get())
        content::ChildThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}
#endif  // defined(OS_WIN)

}  // namespace

void InitializePDF() {
#if defined(OS_WIN)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  bool is_pdf_utility_process = false;
  if (command_line.HasSwitch(service_manager::switches::kServiceSandboxType)) {
    std::string service_sandbox_type = command_line.GetSwitchValueASCII(
        service_manager::switches::kServiceSandboxType);
    if (service_sandbox_type ==
        service_manager::switches::kPdfCompositorSandbox) {
      is_pdf_utility_process = true;
    }
  }
  // On Win10, pdf does not use GDI fonts and does not need to run this
  // initialization for the ppapi process. Printing does still use GDI for
  // fonts on Win10 though.
  if (is_pdf_utility_process ||
      (process_type == switches::kPpapiPluginProcess &&
       base::win::GetVersion() < base::win::Version::WIN10)) {
    // Need to patch a few functions for font loading to work correctly. This
    // can be removed once we switch PDF to use Skia
    // (https://bugs.chromium.org/p/pdfium/issues/detail?id=11).
#if defined(COMPONENT_BUILD)
    HMODULE module = ::GetModuleHandleA("pdfium.dll");
    DCHECK(module);
#else
    HMODULE module = CURRENT_MODULE();
#endif  // defined(COMPONENT_BUILD)

    static base::NoDestructor<base::win::IATPatchFunction> patch_get_font_data;
    patch_get_font_data->PatchFromModule(
        module, "gdi32.dll", "GetFontData",
        reinterpret_cast<void*>(GetFontDataPatch));
    g_original_get_font_data = reinterpret_cast<GetFontDataPtr>(
        patch_get_font_data->original_function());
  }
#endif  // defined(OS_WIN)
}
