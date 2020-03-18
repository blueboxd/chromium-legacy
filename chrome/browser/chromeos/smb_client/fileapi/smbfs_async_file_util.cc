// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/fileapi/smbfs_async_file_util.h"

#include "base/logging.h"
#include "storage/browser/file_system/local_file_util.h"

namespace chromeos {
namespace smb_client {

SmbFsAsyncFileUtil::SmbFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(new storage::LocalFileUtil), profile_(profile) {
  DCHECK(profile_);
}

SmbFsAsyncFileUtil::~SmbFsAsyncFileUtil() = default;

}  // namespace smb_client
}  // namespace chromeos
