// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_FILEAPI_SMBFS_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_FILEAPI_SMBFS_ASYNC_FILE_UTIL_H_

#include "storage/browser/file_system/async_file_util_adapter.h"

class Profile;

namespace chromeos {
namespace smb_client {

class SmbFsAsyncFileUtil : public storage::AsyncFileUtilAdapter {
 public:
  explicit SmbFsAsyncFileUtil(Profile* profile);
  ~SmbFsAsyncFileUtil() override;

  SmbFsAsyncFileUtil() = delete;
  SmbFsAsyncFileUtil(const SmbFsAsyncFileUtil&) = delete;
  SmbFsAsyncFileUtil& operator=(const SmbFsAsyncFileUtil&) = delete;

 private:
  Profile* const profile_;
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_FILEAPI_SMBFS_ASYNC_FILE_UTIL_H_
