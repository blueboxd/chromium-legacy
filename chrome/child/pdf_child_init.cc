// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/pdf_child_init.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/no_destructor.h"
#include "base/win/current_module.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/public/child/child_thread.h"
#endif

namespace {

#if defined(OS_WIN)
HDC WINAPI CreateDCAPatch(LPCSTR driver_name,
                          LPCSTR device_name,
                          LPCSTR output,
                          const void* init_data) {
  DCHECK(std::string("DISPLAY") == std::string(driver_name));
  DCHECK(!device_name);
  DCHECK(!output);
  DCHECK(!init_data);

  // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
  return CreateCompatibleDC(NULL);
}

typedef DWORD (WINAPI* GetFontDataPtr) (HDC hdc,
                                        DWORD table,
                                        DWORD offset,
                                        LPVOID buffer,
                                        DWORD length);
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
  // Need to patch a few functions for font loading to work correctly. This can
  // be removed once we switch PDF to use Skia
  // (https://bugs.chromium.org/p/pdfium/issues/detail?id=11).
#if defined(COMPONENT_BUILD)
  HMODULE module = ::GetModuleHandleA("pdfium.dll");
  DCHECK(module);
#else
  HMODULE module = CURRENT_MODULE();
#endif  // defined(COMPONENT_BUILD)

  // When GDI is delayloaded, calls into real GDI after these patches are
  // created result in the delayload handler overwriting the patches when it
  // resolves the delayed imports with the full import information. To
  // workaround this, we first call into GDI so that the delayed imports get
  // resolved, THEN we apply the patches.
  // On Win10 this isn't necessary because these methods are not used.
  // TODO(crbug.com/988164): Implement a generic IATPatchFunction fix to handle
  // these situations.
  // TODO(crbug.com/988169): This workaround shouldn't be necessary on Win8
  // either, but is because the win32k process mitigations aren't working
  // properly.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    GetFontData(nullptr, 0, 0, nullptr, 0);

  static base::NoDestructor<base::win::IATPatchFunction> patch_createdca;
  patch_createdca->PatchFromModule(module, "gdi32.dll", "CreateDCA",
                                   reinterpret_cast<void*>(CreateDCAPatch));

  static base::NoDestructor<base::win::IATPatchFunction> patch_get_font_data;
  patch_get_font_data->PatchFromModule(
      module, "gdi32.dll", "GetFontData",
      reinterpret_cast<void*>(GetFontDataPatch));
  g_original_get_font_data = reinterpret_cast<GetFontDataPtr>(
      patch_get_font_data->original_function());
#endif  // defined(OS_WIN)
}
