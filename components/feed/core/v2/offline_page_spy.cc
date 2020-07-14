// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/offline_page_spy.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "components/feed/core/v2/algorithm.h"
#include "components/feed/core/v2/surface_updater.h"

namespace feed {

namespace {

std::vector<OfflinePageSpy::BadgeInfo> GetBadgesInStream(
    StreamModel* stream_model) {
  std::vector<OfflinePageSpy::BadgeInfo> badges;
  for (ContentRevision content_rev : stream_model->GetContentList()) {
    const feedstore::Content* content = stream_model->FindContent(content_rev);
    if (!content)
      continue;
    for (const feedwire::PrefetchMetadata& prefetch_metadata :
         content->prefetch_metadata()) {
      const std::string& badge_id = prefetch_metadata.badge_id();
      if (badge_id.empty())
        continue;
      GURL url(prefetch_metadata.uri());
      if (url.is_empty() || !url.is_valid())
        continue;

      badges.emplace_back(url, badge_id);
    }
  }
  return badges;
}

base::flat_set<GURL> OfflineItemsToUrlSet(
    const std::vector<offline_pages::OfflinePageItem>& items) {
  std::vector<GURL> url_list;
  for (const auto& item : items) {
    url_list.push_back(item.GetOriginalUrl());
  }
  return url_list;
}

}  // namespace

OfflinePageSpy::BadgeInfo::BadgeInfo() = default;
OfflinePageSpy::BadgeInfo::BadgeInfo(const GURL& url,
                                     const std::string& badge_id)
    : url(url), badge_id(badge_id) {}

bool OfflinePageSpy::BadgeInfo::operator<(const BadgeInfo& rhs) const {
  return std::tie(url, badge_id) < std::tie(rhs.url, rhs.badge_id);
}

OfflinePageSpy::OfflinePageSpy(
    SurfaceUpdater* surface_updater,
    offline_pages::OfflinePageModel* offline_page_model)
    : surface_updater_(surface_updater),
      offline_page_model_(offline_page_model) {
  offline_page_model_->AddObserver(this);
}

OfflinePageSpy::~OfflinePageSpy() {
  offline_page_model_->RemoveObserver(this);
}

void OfflinePageSpy::SetModel(StreamModel* stream_model) {
  if (stream_model_) {
    stream_model_->RemoveObserver(this);
    stream_model_ = nullptr;
  }
  if (stream_model) {
    stream_model_ = stream_model;
    stream_model_->AddObserver(this);
    UpdateWatchedPages();
  } else {
    for (const BadgeInfo& badge : badges_) {
      if (badge.available_offline) {
        surface_updater_->SetOfflinePageAvailability(badge.badge_id, false);
      }
    }
    badges_.clear();
  }
}

void OfflinePageSpy::SetAvailability(const base::flat_set<GURL>& urls,
                                     bool available) {
  for (BadgeInfo& badge : badges_) {
    if (badge.available_offline != available && urls.contains(badge.url)) {
      badge.available_offline = available;
      surface_updater_->SetOfflinePageAvailability(badge.badge_id, available);
    }
  }
}

void OfflinePageSpy::GetPagesDone(
    const std::vector<offline_pages::OfflinePageItem>& items) {
  SetAvailability(OfflineItemsToUrlSet(items), true);
}

void OfflinePageSpy::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  SetAvailability({added_page.GetOriginalUrl()}, true);
}

void OfflinePageSpy::OfflinePageDeleted(
    const offline_pages::OfflinePageItem& deleted_page) {
  SetAvailability({deleted_page.GetOriginalUrl()}, false);
}

void OfflinePageSpy::OnUiUpdate(const StreamModel::UiUpdate& update) {
  DCHECK(stream_model_);
  if (update.content_list_changed)
    UpdateWatchedPages();
}

void OfflinePageSpy::UpdateWatchedPages() {
  std::vector<BadgeInfo> badges = GetBadgesInStream(stream_model_);

  // Both lists need to be sorted. |badges_| should already be sorted.
  std::sort(badges.begin(), badges.end());
  DCHECK(std::is_sorted(badges_.begin(), badges_.end()));

  // Compare new and old lists. We need to inform SurfaceUpdater of removed
  // badges, and collect new URLs.
  std::vector<GURL> new_urls;
  auto differ = [&](BadgeInfo* new_badge, BadgeInfo* old_badge) {
    if (!old_badge) {  // Added a page.
      new_urls.push_back(new_badge->url);
      return;
    }
    if (!new_badge) {  // Removed a page.
      surface_updater_->SetOfflinePageAvailability(old_badge->badge_id, false);
      return;
    }
    // Page remains, update |badges|.
    new_badge->available_offline = old_badge->available_offline;
  };

  DiffSortedRange(badges.begin(), badges.end(), badges_.begin(), badges_.end(),
                  differ);

  badges_ = std::move(badges);
  RequestOfflinePageStatus(std::move(new_urls));
}

void OfflinePageSpy::RequestOfflinePageStatus(std::vector<GURL> new_urls) {
  if (new_urls.empty())
    return;

  offline_pages::PageCriteria criteria;
  criteria.exclude_tab_bound_pages = true;
  criteria.additional_criteria = base::BindRepeating(
      [](const base::flat_set<GURL>& url_set,
         const offline_pages::OfflinePageItem& item) {
        return url_set.count(item.GetOriginalUrl()) > 0;
      },
      std::move(new_urls));

  offline_page_model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&OfflinePageSpy::GetPagesDone, GetWeakPtr()));
}

}  // namespace feed
