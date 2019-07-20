// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/service_tab_launcher.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#else
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#endif

using offline_items_collection::ContentId;
using offline_items_collection::LaunchLocation;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemFilter;


namespace {

constexpr char kProviderNamespace[] = "content_index";
constexpr char kEntryKeySeparator[] = "#";

struct EntryKeyComponents {
  int64_t service_worker_registration_id;
  url::Origin origin;
  std::string description_id;
};

std::string EntryKey(int64_t service_worker_registration_id,
                     const url::Origin& origin,
                     const std::string& description_id) {
  return base::NumberToString(service_worker_registration_id) +
         kEntryKeySeparator + origin.GetURL().spec() + kEntryKeySeparator +
         description_id;
}

std::string EntryKey(const content::ContentIndexEntry& entry) {
  return EntryKey(entry.service_worker_registration_id,
                  url::Origin::Create(entry.launch_url.GetOrigin()),
                  entry.description->id);
}

EntryKeyComponents GetEntryKeyComponents(const std::string& key) {
  size_t pos1 = key.find_first_of(kEntryKeySeparator);
  DCHECK_NE(pos1, std::string::npos);
  size_t pos2 = key.find_first_of(kEntryKeySeparator, pos1 + 1);
  DCHECK_NE(pos2, std::string::npos);

  int64_t service_worker_registration_id = -1;
  base::StringToInt64(base::StringPiece(key.data(), pos1),
                      &service_worker_registration_id);

  GURL origin(key.substr(pos1 + 1, pos2 - pos1 - 1));
  DCHECK(origin.is_valid());

  return {service_worker_registration_id, url::Origin::Create(origin),
          key.substr(pos2 + 1)};
}

OfflineItemFilter CategoryToFilter(blink::mojom::ContentCategory category) {
  switch (category) {
    case blink::mojom::ContentCategory::HOME_PAGE:
    case blink::mojom::ContentCategory::ARTICLE:
      return OfflineItemFilter::FILTER_PAGE;
    case blink::mojom::ContentCategory::VIDEO:
      return OfflineItemFilter::FILTER_VIDEO;
    case blink::mojom::ContentCategory::AUDIO:
      return OfflineItemFilter::FILTER_AUDIO;
  }
}

OfflineItem EntryToOfflineItem(const content::ContentIndexEntry& entry) {
  OfflineItem item;
  item.id = ContentId(kProviderNamespace, EntryKey(entry));
  item.title = entry.description->title;
  item.description = entry.description->description;
  item.filter = CategoryToFilter(entry.description->category);
  item.is_transient = false;
  item.is_suggested = true;
  item.creation_time = entry.registration_time;
  item.is_openable = true;
  item.state = offline_items_collection::OfflineItemState::COMPLETE;
  item.is_resumable = false;
  item.can_rename = false;
  item.page_url = entry.launch_url;

  return item;
}

}  // namespace

ContentIndexProviderImpl::ContentIndexProviderImpl(Profile* profile)
    : profile_(profile),
      aggregator_(OfflineContentAggregatorFactory::GetForKey(
          profile_->GetProfileKey())),
      weak_ptr_factory_(this) {
  aggregator_->RegisterProvider(kProviderNamespace, this);
}

ContentIndexProviderImpl::~ContentIndexProviderImpl() {
  if (aggregator_)
    aggregator_->UnregisterProvider(kProviderNamespace);
}

void ContentIndexProviderImpl::Shutdown() {
  aggregator_->UnregisterProvider(kProviderNamespace);
  aggregator_ = nullptr;
}

void ContentIndexProviderImpl::OnContentAdded(
    content::ContentIndexEntry entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string entry_key = EntryKey(entry);
  auto offline_item = EntryToOfflineItem(entry);

  bool is_update = entries_.erase(entry_key);

  if (is_update) {
    offline_items_collection::UpdateDelta delta;
    delta.visuals_changed = true;
    for (auto& observer : observers_)
      observer.OnItemUpdated(offline_item, delta);
  } else {
    OfflineItemList items(1, offline_item);
    for (auto& observer : observers_)
      observer.OnItemsAdded(items);
  }

  entries_.emplace(std::move(entry_key), std::move(offline_item));
}

void ContentIndexProviderImpl::OnContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string entry_key =
      EntryKey(service_worker_registration_id, origin, description_id);
  auto did_erase = entries_.erase(entry_key);

  if (did_erase) {
    ContentId id(kProviderNamespace, entry_key);
    for (auto& observer : observers_)
      observer.OnItemRemoved(id);
  }
}

void ContentIndexProviderImpl::OpenItem(LaunchLocation location,
                                        const ContentId& id) {
  auto it = entries_.find(id.id);
  if (it == entries_.end())
    return;

#if defined(OS_ANDROID)
  content::OpenURLParams params(it->second.page_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /* is_renderer_initiated= */ false);
  ServiceTabLauncher::GetInstance()->LaunchTab(profile_, params,
                                               base::DoNothing());
#else
  NavigateParams nav_params(profile_, it->second.page_url,
                            ui::PAGE_TRANSITION_LINK);
  Navigate(&nav_params);
#endif
}

void ContentIndexProviderImpl::RemoveItem(const ContentId& id) {
  NOTIMPLEMENTED();
}

void ContentIndexProviderImpl::CancelDownload(const ContentId& id) {
  NOTREACHED();
}

void ContentIndexProviderImpl::PauseDownload(const ContentId& id) {
  NOTREACHED();
}

void ContentIndexProviderImpl::ResumeDownload(const ContentId& id,
                                              bool has_user_gesture) {
  NOTREACHED();
}

void ContentIndexProviderImpl::GetItemById(const ContentId& id,
                                           SingleItemCallback callback) {
  auto it = entries_.find(id.id);
  if (it == entries_.end())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), it->second));
}

void ContentIndexProviderImpl::GetAllItems(MultipleItemCallback callback) {
  OfflineItemList list;
  for (const auto& entry : entries_)
    list.push_back(entry.second);

  // TODO(crbug.com/973844): Consider fetching these from the DB rather than
  // storing them in memory.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(list)));
}

void ContentIndexProviderImpl::GetVisualsForItem(const ContentId& id,
                                                 GetVisualsOptions options,
                                                 VisualsCallback callback) {
  auto components = GetEntryKeyComponents(id.id);

  auto* storage_partition = content::BrowserContext::GetStoragePartitionForSite(
      profile_, components.origin.GetURL(), /* can_create= */ false);

  if (!storage_partition || !storage_partition->GetContentIndexContext()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  storage_partition->GetContentIndexContext()->GetIcon(
      components.service_worker_registration_id, components.description_id,
      base::BindOnce(&ContentIndexProviderImpl::DidGetIcon,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void ContentIndexProviderImpl::DidGetIcon(const ContentId& id,
                                          VisualsCallback callback,
                                          SkBitmap icon) {
  auto visuals =
      std::make_unique<offline_items_collection::OfflineItemVisuals>();
  visuals->icon = gfx::Image::CreateFrom1xBitmap(icon);
  std::move(callback).Run(id, std::move(visuals));
}

void ContentIndexProviderImpl::GetShareInfoForItem(const ContentId& id,
                                                   ShareCallback callback) {
  NOTIMPLEMENTED();
}

void ContentIndexProviderImpl::RenameItem(const ContentId& id,
                                          const std::string& name,
                                          RenameCallback callback) {
  NOTREACHED();
}

void ContentIndexProviderImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContentIndexProviderImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
