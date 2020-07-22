// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_
#define CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_

namespace mini_installer {

class MiniFile;

// Expands the first file in |source| to the file |destination| using
// Microsoft's MSCF compression algorithm (a la expand.exe). Returns true on
// success, in which case |file| holds an open handle to the destination file.
// |file| will be opened with exclusive write access and shared read and delete
// access, and will be marked as delete-on-close.
bool Expand(const wchar_t* source, const wchar_t* destination, MiniFile& file);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_DECOMPRESS_H_
