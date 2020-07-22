// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_

#include <windows.h>

#include "chrome/installer/mini_installer/path_string.h"

namespace mini_installer {

// A simple abstraction over a path to a file and a Windows file handle to it.
class MiniFile {
 public:
  enum class DeleteOnClose : bool { kNo = false, kYes = true };
  explicit MiniFile(DeleteOnClose delete_on_close);

  // Closes the file if the instance holds a valid handle. The file will be
  // deleted if the instance was constructed with |delete_on_close|.
  ~MiniFile();

  MiniFile(const MiniFile&) = delete;
  MiniFile(MiniFile&&) = delete;
  MiniFile& operator=(const MiniFile&) = delete;

  // Postcondition: other.path() will return an empty string and other.IsValid()
  // will return false.
  MiniFile& operator=(MiniFile&& other) noexcept;

  // Creates a new file at |path| for exclusive writing. Returns true if the
  // file was created, in which case IsValid() will return true.
  bool Create(const wchar_t* path);

  // Returns true if this object has a path and a handle to an open file.
  bool IsValid() const;

  // Drops write permission on the file handle so that other parties that
  // require no writers may open the file. In particular, the Windows loader
  // opens files for execution with shared read/delete access, as do the
  // extraction operations in Chrome's mini_installer.exe and setup.exe. These
  // would fail with sharing violations if mini_installer were to hold files
  // open with write permissions. Returns false on failure, in which case the
  // instance is no longer valid.  The file will have been deleted if the
  // instance was created with DeleteOnClose.
  bool DropWritePermission();

  // Closes the handle and clears the path. The file will be deleted if the
  // instance was constructed with |delete_on_close|. Following this, IsValid()
  // will return false.
  void Close();

  // Returns a new handle to the file, or INVALID_HANDLE_VALUE on error.
  HANDLE DuplicateHandle() const;

  // Opens the file for read access, disallowing writers (as if Create followed
  // by DropWritePermission).
  bool Open(const PathString& path);

  // Returns the path to the open file, or a pointer to an empty string if
  // IsValid() is false.
  const wchar_t* path() const { return path_.get(); }

  // Returns the open file handle. The caller must not close it, and must not
  // refer to it after this instance is closed or destroyed.
  HANDLE GetHandleUnsafe() const;

 private:
  // The path by which |handle_| was created or opened, or an empty path if
  // |handle_| is not valid.
  PathString path_;

  // A handle to the open file, or INVALID_HANDLE_VALUE.
  HANDLE handle_ = INVALID_HANDLE_VALUE;

  // Zero or FILE_FLAG_DELETE_ON_CLOSE, according to how the instance was
  // constructed.
  const DWORD delete_on_close_flag_;
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MINI_FILE_H_
