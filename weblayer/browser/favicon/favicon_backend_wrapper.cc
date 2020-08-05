// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_backend_wrapper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "components/favicon/core/favicon_backend.h"
#include "components/favicon/core/favicon_database.h"

namespace weblayer {

FaviconBackendWrapper::FaviconBackendWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : base::RefCountedDeleteOnSequence<FaviconBackendWrapper>(task_runner),
      task_runner_(task_runner) {}

void FaviconBackendWrapper::Init(const base::FilePath& db_path) {
  favicon_backend_ = favicon::FaviconBackend::Create(db_path, this);
  if (!favicon_backend_) {
    LOG(WARNING) << "Could not initialize the favicon database.";

    // The favicon db is not critical. On failure initializing try deleting
    // the file and repeating. Note that FaviconDatabase already tries to
    // initialize twice.
    base::DeleteFile(db_path);

    favicon_backend_ = favicon::FaviconBackend::Create(db_path, this);
    if (!favicon_backend_) {
      LOG(WARNING) << "Could not initialize db second time, giving up.";
      return;
    }
  }
}

void FaviconBackendWrapper::Shutdown() {
  // Ensures there isn't a reference to this in the task runner (by way of the
  // task the timer posts).
  commit_timer_.Stop();
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::GetFaviconsForUrl(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetFaviconsForUrl(page_url, icon_types,
                                             desired_sizes,
                                             /* fallback_to_host */ false);
}

void FaviconBackendWrapper::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  if (favicon_backend_)
    favicon_backend_->SetFaviconsOutOfDateForPage(page_url);
}

void FaviconBackendWrapper::SetFavicons(const base::flat_set<GURL>& page_urls,
                                        favicon_base::IconType icon_type,
                                        const GURL& icon_url,
                                        const std::vector<SkBitmap>& bitmaps) {
  if (favicon_backend_) {
    favicon_backend_->SetFavicons(page_urls, icon_type, icon_url, bitmaps,
                                  favicon::FaviconBitmapType::ON_VISIT);
  }
}

void FaviconBackendWrapper::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  if (!favicon_backend_)
    return;

  std::set<GURL> changed_urls = favicon_backend_->CloneFaviconMappingsForPages(
      {page_url_to_read}, icon_types, page_urls_to_write);
  if (!changed_urls.empty())
    ScheduleCommitForFavicons();
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::GetFavicon(const GURL& icon_url,
                                  favicon_base::IconType icon_type,
                                  const std::vector<int>& desired_sizes) {
  return UpdateFaviconMappingsAndFetch({}, icon_url, icon_type, desired_sizes);
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->UpdateFaviconMappingsAndFetch(
      page_urls, icon_url, icon_type, desired_sizes);
}

void FaviconBackendWrapper::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  if (!favicon_backend_)
    favicon_backend_->DeleteFaviconMappings(page_urls, icon_type);
}

void FaviconBackendWrapper::ScheduleCommitForFavicons() {
  if (!commit_timer_.IsRunning()) {
    // 10 seconds matches that of HistoryBackend.
    commit_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(10), this,
                        &FaviconBackendWrapper::Commit);
  }
}

std::vector<GURL> FaviconBackendWrapper::GetCachedRecentRedirectsForPage(
    const GURL& page_url) {
  // By only returning |page_url| this code won't set the favicon on redirects.
  // If that becomes necessary, we would need this class to know about
  // redirects. Chrome does this by way of HistoryService remembering redirects
  // for recent pages. See |HistoryBackend::recent_redirects_|.
  return {page_url};
}

void FaviconBackendWrapper::OnFaviconChangedForPageAndRedirects(
    const GURL& page_url) {
  // Nothing to do here as WebLayer doesn't notify of favicon changes through
  // this code path.
}

FaviconBackendWrapper::~FaviconBackendWrapper() = default;

void FaviconBackendWrapper::Commit() {
  if (favicon_backend_)
    favicon_backend_->Commit();
}

}  // namespace weblayer
