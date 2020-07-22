// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_file.h"

#include <utility>

namespace mini_installer {

MiniFile::MiniFile(DeleteOnClose delete_on_close)
    : delete_on_close_flag_(delete_on_close != DeleteOnClose::kNo
                                ? FILE_FLAG_DELETE_ON_CLOSE
                                : 0) {}

MiniFile::~MiniFile() {
  Close();
}

MiniFile& MiniFile::operator=(MiniFile&& other) noexcept {
  Close();
  path_.assign(other.path_);
  other.path_.clear();
  handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
  return *this;
}

bool MiniFile::Create(const wchar_t* path) {
  Close();
  if (!path_.assign(path))
    return false;
  handle_ = ::CreateFileW(path_.get(), GENERIC_WRITE,
                          FILE_SHARE_DELETE | FILE_SHARE_READ,
                          /*lpSecurityAttributes=*/nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL | delete_on_close_flag_,
                          /*hTemplateFile=*/nullptr);
  if (handle_ != INVALID_HANDLE_VALUE)
    return true;
  path_.clear();
  return false;
}

bool MiniFile::IsValid() const {
  return handle_ != INVALID_HANDLE_VALUE;
}

bool MiniFile::DropWritePermission() {
  // The original file was opened with write access (of course), so it will take
  // a little hoop jumping to get a handle without it. First, get a new handle
  // that doesn't have write access. This one must allow others to write on
  // account of the fact that the original handle has write access.
  HANDLE without_write =
      ::ReOpenFile(handle_, GENERIC_READ,
                   FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ,
                   delete_on_close_flag_);
  if (without_write == INVALID_HANDLE_VALUE) {
    const auto error = ::GetLastError();
    Close();
    ::SetLastError(error);
    return false;
  }

  // Next, close the original handle so that there are no longer any writers.
  // This will mark the file for deletion if the original handle was opened with
  // FILE_FLAG_DELETE_ON_CLOSE.
  ::CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));

  // Now unmark the file for deletion if needed.
  if (delete_on_close_flag_) {
    FILE_DISPOSITION_INFO disposition = {/*DeleteFile=*/FALSE};
    if (!::SetFileInformationByHandle(without_write, FileDispositionInfo,
                                      &disposition, sizeof(disposition))) {
      const auto error = ::GetLastError();
      ::CloseHandle(std::exchange(without_write, INVALID_HANDLE_VALUE));
      Close();
      ::SetLastError(error);
      return false;
    }
  }

  // Now open a read-only handle (with FILE_FLAG_DELETE_ON_CLOSE as needed) that
  // doesn't allow others to write. Note that there is a potential race here:
  // another party could open the file for shared write access at this precise
  // moment, causing this ReOpenFile to fail. This would likely be an issue
  // anyway, as one common thing to do with the file is to execute it, which
  // will fail if there are writers.
  handle_ =
      ::ReOpenFile(without_write, GENERIC_READ,
                   FILE_SHARE_DELETE | FILE_SHARE_READ, delete_on_close_flag_);
  if (handle_ == INVALID_HANDLE_VALUE) {
    const auto error = ::GetLastError();
    ::CloseHandle(std::exchange(without_write, INVALID_HANDLE_VALUE));
    Close();
    ::SetLastError(error);
    return false;
  }

  // Closing the handle that allowed shared writes may once again mark the file
  // for deletion.
  ::CloseHandle(std::exchange(without_write, INVALID_HANDLE_VALUE));

  // Everything went according to plan; |handle_| is now lacking write access
  // and does not allow other writers. The last step is to unmark the file for
  // deletion once again, as the closure of |without_write| has re-marked it.
  if (delete_on_close_flag_) {
    FILE_DISPOSITION_INFO disposition = {/*DeleteFile=*/FALSE};
    if (!::SetFileInformationByHandle(handle_, FileDispositionInfo,
                                      &disposition, sizeof(disposition))) {
      const auto error = ::GetLastError();
      Close();
      ::SetLastError(error);
      return false;
    }
  }

  return true;
}

void MiniFile::Close() {
  if (IsValid())
    ::CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));
  path_.clear();
}

HANDLE MiniFile::DuplicateHandle() const {
  if (!IsValid())
    return INVALID_HANDLE_VALUE;
  HANDLE handle = INVALID_HANDLE_VALUE;
  return ::DuplicateHandle(::GetCurrentProcess(), handle_,
                           ::GetCurrentProcess(), &handle,
                           /*dwDesiredAccess=*/0,
                           /*bInerhitHandle=*/FALSE, DUPLICATE_SAME_ACCESS)
             ? handle
             : INVALID_HANDLE_VALUE;
}

bool MiniFile::Open(const PathString& path) {
  Close();
  handle_ = ::CreateFileW(path.get(), GENERIC_READ,
                          FILE_SHARE_DELETE | FILE_SHARE_READ,
                          /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                          delete_on_close_flag_, /*hTemplateFile=*/nullptr);
  if (handle_ == INVALID_HANDLE_VALUE)
    return false;
  path_.assign(path);
  return true;
}

HANDLE MiniFile::GetHandleUnsafe() const {
  return handle_;
}

}  // namespace mini_installer
