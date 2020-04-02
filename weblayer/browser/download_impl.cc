// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/download/public/common/download_item.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/DownloadImpl_jni.h"
#endif

namespace weblayer {

namespace {
const char kDownloadImplKeyName[] = "weblayer_download_impl";
}

void DownloadImpl::Create(download::DownloadItem* item) {
  item->SetUserData(kDownloadImplKeyName,
                    base::WrapUnique(new DownloadImpl(item)));
}

DownloadImpl* DownloadImpl::Get(download::DownloadItem* item) {
  return static_cast<DownloadImpl*>(item->GetUserData(kDownloadImplKeyName));
}

DownloadImpl::DownloadImpl(download::DownloadItem* item) : item_(item) {}

DownloadImpl::~DownloadImpl() {
#if defined(OS_ANDROID)
  if (java_download_) {
    Java_DownloadImpl_onNativeDestroyed(base::android::AttachCurrentThread(),
                                        java_download_);
  }
#endif
}

#if defined(OS_ANDROID)
void DownloadImpl::SetJavaDownload(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_download) {
  java_download_.Reset(env, java_download);
}

base::android::ScopedJavaLocalRef<jstring> DownloadImpl::GetLocation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetLocation().value()));
}

base::android::ScopedJavaLocalRef<jstring> DownloadImpl::GetMimeTypeImpl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetMimeType()));
}
#endif

DownloadState DownloadImpl::GetState() {
  if (item_->GetState() == download::DownloadItem::COMPLETE)
    return DownloadState::kComplete;

  if (cancel_pending_ || item_->GetState() == download::DownloadItem::CANCELLED)
    return DownloadState::kCancelled;

  if (pause_pending_ || (item_->IsPaused() && !resume_pending_))
    return DownloadState::kPaused;

  if (resume_pending_ ||
      item_->GetState() == download::DownloadItem::IN_PROGRESS) {
    return DownloadState::kInProgress;
  }

  return DownloadState::kFailed;
}

int64_t DownloadImpl::GetTotalBytes() {
  return item_->GetTotalBytes();
}

int64_t DownloadImpl::GetReceivedBytes() {
  return item_->GetReceivedBytes();
}

void DownloadImpl::Pause() {
  // The Pause/Resume/Cancel methods need to be called in a PostTask because we
  // may be in a callback from the download subsystem and it doesn't handle
  // nested calls.
  resume_pending_ = false;
  pause_pending_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadImpl::PauseInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DownloadImpl::Resume() {
  pause_pending_ = false;
  resume_pending_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadImpl::ResumeInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DownloadImpl::Cancel() {
  cancel_pending_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadImpl::CancelInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath DownloadImpl::GetLocation() {
  return item_->GetTargetFilePath();
}

std::string DownloadImpl::GetMimeType() {
  return item_->GetMimeType();
}

DownloadError DownloadImpl::GetError() {
  auto reason = item_->GetLastReason();
  if (reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
    return DownloadError::kNoError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM)
    return DownloadError::kSSLError;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED &&
      reason <=
          download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT)
    return DownloadError::kServerError;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED &&
      reason <= download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST)
    return DownloadError::kConnectivityError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE)
    return DownloadError::kNoSpace;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED &&
      reason <= download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE)
    return DownloadError::kFileError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED)
    return DownloadError::kCancelled;

  return DownloadError::kOtherError;
}

void DownloadImpl::PauseInternal() {
  if (pause_pending_) {
    pause_pending_ = false;
    item_->Pause();
  }
}

uint32_t DownloadImpl::GetId() {
  return item_->GetId();
}

void DownloadImpl::ResumeInternal() {
  if (resume_pending_) {
    resume_pending_ = false;
    item_->Resume(true);
  }
}

void DownloadImpl::CancelInternal() {
  cancel_pending_ = false;
  item_->Cancel(true);
}

}  // namespace weblayer
