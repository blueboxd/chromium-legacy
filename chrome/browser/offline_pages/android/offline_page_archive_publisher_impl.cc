// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_archive_publisher_impl.h"

#include <errno.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/android/chrome_jni_headers/OfflinePageArchivePublisherBridge_jni.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_store_utils.h"

using base::android::ScopedJavaLocalRef;

namespace offline_pages {

namespace {

using offline_pages::SavePageResult;

// Creates a singleton Delegate.
OfflinePageArchivePublisherImpl::Delegate* GetDefaultDelegate() {
  static OfflinePageArchivePublisherImpl::Delegate delegate;
  return &delegate;
}

// Helper function to do the move and register synchronously. Make sure this is
// called from a background thread.
PublishArchiveResult MoveAndRegisterArchive(
    const offline_pages::OfflinePageItem& offline_page,
    const base::FilePath& publish_directory,
    OfflinePageArchivePublisherImpl::Delegate* delegate) {
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
  if (!delegate->IsDownloadManagerInstalled()) {
    archive_result.move_result = SavePageResult::ADD_TO_DOWNLOAD_MANAGER_FAILED;
    return archive_result;
  }

  // TODO(petewil): Handle empty page title.
  std::string page_title = base::UTF16ToUTF8(offline_page.title);
  // We use the title for a description, since the add to the download manager
  // fails without a description, and we don't have anything better to use.
  int64_t download_id = delegate->AddCompletedDownload(
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

OfflinePageArchivePublisherImpl::OfflinePageArchivePublisherImpl(
    ArchiveManager* archive_manager)
    : archive_manager_(archive_manager), delegate_(GetDefaultDelegate()) {}

OfflinePageArchivePublisherImpl::~OfflinePageArchivePublisherImpl() {}

void OfflinePageArchivePublisherImpl::SetDelegateForTesting(
    OfflinePageArchivePublisherImpl::Delegate* delegate) {
  delegate_ = delegate;
}

void OfflinePageArchivePublisherImpl::PublishArchive(
    const OfflinePageItem& offline_page,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    PublishArchiveDoneCallback publish_done_callback) const {
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(), FROM_HERE,
      base::BindOnce(&MoveAndRegisterArchive, offline_page,
                     archive_manager_->GetPublicArchivesDir(), delegate_),
      base::BindOnce(std::move(publish_done_callback), offline_page));
}

void OfflinePageArchivePublisherImpl::UnpublishArchives(
    const std::vector<int64_t>& download_manager_ids) const {
  delegate_->Remove(download_manager_ids);
}

// Delegate implementation using Android download manager.

bool OfflinePageArchivePublisherImpl::Delegate::IsDownloadManagerInstalled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jboolean is_installed =
      Java_OfflinePageArchivePublisherBridge_isAndroidDownloadManagerInstalled(
          env);
  return is_installed;
}

int64_t OfflinePageArchivePublisherImpl::Delegate::AddCompletedDownload(
    const std::string& title,
    const std::string& description,
    const std::string& path,
    int64_t length,
    const std::string& uri,
    const std::string& referer) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Convert strings to jstring references.
  ScopedJavaLocalRef<jstring> j_title =
      base::android::ConvertUTF8ToJavaString(env, title);
  ScopedJavaLocalRef<jstring> j_description =
      base::android::ConvertUTF8ToJavaString(env, description);
  ScopedJavaLocalRef<jstring> j_path =
      base::android::ConvertUTF8ToJavaString(env, path);
  ScopedJavaLocalRef<jstring> j_uri =
      base::android::ConvertUTF8ToJavaString(env, uri);
  ScopedJavaLocalRef<jstring> j_referer =
      base::android::ConvertUTF8ToJavaString(env, referer);

  return Java_OfflinePageArchivePublisherBridge_addCompletedDownload(
      env, j_title, j_description, j_path, length, j_uri, j_referer);
}

int OfflinePageArchivePublisherImpl::Delegate::Remove(
    const std::vector<int64_t>& android_download_manager_ids) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Build a JNI array with our ID data.
  ScopedJavaLocalRef<jlongArray> j_ids =
      base::android::ToJavaLongArray(env, android_download_manager_ids);

  return Java_OfflinePageArchivePublisherBridge_remove(env, j_ids);
}

}  // namespace offline_pages
