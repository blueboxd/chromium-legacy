// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE_H_

#include <windows.h>

#include <stddef.h>

namespace mini_installer {

class MiniFile;

// This class models a windows PE resource. It does not pretend to be a full
// API wrapper and it is just concerned with loading it to memory and writing
// it to disk. Each resource is unique only in the context of a loaded module,
// that is why you need to specify one on each constructor.
class PEResource {
 public:
  // This ctor takes the handle to the resource and the module where it was
  // found. Ownership of the resource is transferred to this object.
  PEResource(HRSRC resource, HMODULE module);

  // This ctor takes the resource name, the resource type and the module where
  // to look for the resource. If the resource is found IsValid() returns true.
  PEResource(const wchar_t* name, const wchar_t* type, HMODULE module);

  // Returns true if the resource is valid.
  bool IsValid();

  // Returns the size in bytes of the resource. Returns zero if the resource is
  // not valid.
  size_t Size();

  // Writes the resource to the file |path|. Returns true on success, in which
  // case |file| holds an open handle to the destination file. |file| will be
  // opened with exclusive write access and shared read and delete access, and
  // will be marked as delete-on-close.
  bool WriteToDisk(const wchar_t* path, MiniFile& file);

 private:
  HRSRC resource_;
  HMODULE module_;
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_PE_RESOURCE_H_
