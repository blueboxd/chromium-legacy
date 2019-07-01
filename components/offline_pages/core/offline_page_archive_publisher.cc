// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_archive_publisher.h"

#include <errno.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/system_download_manager.h"

namespace offline_pages {

namespace {

using offline_pages::SavePageResult;

// Helper function to do the move and register synchronously. Make sure this is
// called from a background thread.
PublishArchiveResult MoveAndRegisterArchive(
    const offline_pages::OfflinePageItem& offline_page,
    const base::FilePath& publish_directory,
    offline_pages::SystemDownloadManager* download_manager) {
  PublishArchiveResult archive_result;
  // Calculate the new file name.
  base::FilePath new_file_path =
      offline_pages::model_utils::GenerateUniqueFilenameForOfflinePage(
          offline_page.title, offline_page.url, publish_directory);

  // Create the destination directory if it does not already exist.
  if (!publish_directory.empty() && !base::DirectoryExists(publish_directory)) {
    base::File::Error file_error;
    base::CreateDirectoryAndGetError(publish_directory, &file_error);
  }

  // Move the file.
  bool moved = base::Move(offline_page.file_path, new_file_path);
  if (!moved) {
    archive_result.move_result = SavePageResult::FILE_MOVE_FAILED;
    DVPLOG(0) << "OfflinePage publishing file move failure " << __func__;

    if (!base::PathExists(offline_page.file_path)) {
      DVLOG(0) << "Can't copy from non-existent path, from "
               << offline_page.file_path << " " << __func__;
    }
    if (!base::PathExists(publish_directory)) {
      DVLOG(0) << "Target directory does not exist, " << publish_directory
               << " " << __func__;
    }
    return archive_result;
  }

  // Tell the download manager about our file, get back an id.
  if (!download_manager->IsDownloadManagerInstalled()) {
    archive_result.move_result = SavePageResult::ADD_TO_DOWNLOAD_MANAGER_FAILED;
    return archive_result;
  }

  // TODO(petewil): Handle empty page title.
  std::string page_title = base::UTF16ToUTF8(offline_page.title);
  // We use the title for a description, since the add to the download manager
  // fails without a description, and we don't have anything better to use.
  int64_t download_id = download_manager->AddCompletedDownload(
      page_title, page_title,
      offline_pages::store_utils::ToDatabaseFilePath(new_file_path),
      offline_page.file_size, offline_page.url.spec(), std::string());
  if (download_id == 0LL) {
    archive_result.move_result = SavePageResult::ADD_TO_DOWNLOAD_MANAGER_FAILED;
    return archive_result;
  }

  // Put results into the result object.
  archive_result.move_result = SavePageResult::SUCCESS;
  archive_result.new_file_path = new_file_path;
  archive_result.download_id = download_id;

  return archive_result;
}

}  // namespace

OfflinePageArchivePublisher::OfflinePageArchivePublisher(
    ArchiveManager* archive_manager,
    SystemDownloadManager* download_manager)
    : archive_manager_(archive_manager), download_manager_(download_manager) {}

void OfflinePageArchivePublisher::PublishArchive(
    const OfflinePageItem& offline_page,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    PublishArchiveDoneCallback publish_done_callback) const {
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(), FROM_HERE,
      base::BindOnce(&MoveAndRegisterArchive, offline_page,
                     archive_manager_->GetPublicArchivesDir(),
                     download_manager_),
      base::BindOnce(std::move(publish_done_callback), offline_page));
}

}  // namespace offline_pages
