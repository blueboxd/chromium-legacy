// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/fileapi/smbfs_async_file_util.h"

#include <utility>

#include "base/check_op.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/smb_client/smbfs_share.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/local_file_util.h"

namespace chromeos {
namespace smb_client {
namespace {

void AllowCredentialsRequestOnUIThread(Profile* profile,
                                       const base::FilePath& path) {
  SmbService* service = SmbServiceFactory::Get(profile);
  DCHECK(service);
  SmbFsShare* share = service->GetSmbFsShareForPath(path);
  // Because the request is posted from the IO thread, there's no guarantee the
  // share still exists at this point.
  if (share) {
    // To avoid spamming the user with credentials dialogs, we only want to show
    // the dialog when the user clicks on the share in the Files App. However,
    // there's no way to know the request came from the Files App. Instead,
    // intercept ReadDirectory(), which the Files App does whenever the user
    // enters a directory and use that as a proxy for user-initiated navigation.
    // This isn't perfect, since lots of other things are likely to ask for a
    // directory listing. But it also prevents dialog activation by any
    // operation done purely through the native FUSE filesystem.
    share->AllowCredentialsRequest();
  }
}

}  // namespace

SmbFsAsyncFileUtil::SmbFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(new storage::LocalFileUtil), profile_(profile) {
  DCHECK(profile_);
}

SmbFsAsyncFileUtil::~SmbFsAsyncFileUtil() = default;

void SmbFsAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&AllowCredentialsRequestOnUIThread, profile_, url.path()),
      base::BindOnce(&SmbFsAsyncFileUtil::RealReadDirectory,
                     weak_factory_.GetWeakPtr(), std::move(context), url,
                     std::move(callback)));
}

void SmbFsAsyncFileUtil::RealReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  storage::AsyncFileUtilAdapter::ReadDirectory(std::move(context), url,
                                               std::move(callback));
}

}  // namespace smb_client
}  // namespace chromeos
