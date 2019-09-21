// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <set>

#include "base/location.h"
#include "base/strings/string16.h"

namespace content {

bool FontFilePathAndTtcIndex(IDWriteFontFace* font,
                             base::string16& file_path,
                             uint32_t& ttc_index);
bool FontFilePathAndTtcIndex(IDWriteFont* font,
                             base::string16& file_path,
                             uint32_t& ttc_index);
bool AddFilesForFont(IDWriteFont* font,
                     const base::string16& windows_fonts_path,
                     std::set<base::string16>* path_set,
                     std::set<base::string16>* custom_font_path_set,
                     uint32_t* ttc_index);

base::string16 GetWindowsFontsPath();

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_
