// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image_skia.h"

using offline_items_collection::ContentId;
using offline_items_collection::LaunchLocation;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemFilter;

struct ContentIndexProviderImpl::EntryData {
  EntryData() = delete;
  ~EntryData() = default;

  OfflineItem offline_item;
  base::WeakPtr<content::ContentIndexProvider::Client> client;
};

namespace {

constexpr char kProviderNamespace[] = "content_index";
constexpr char kSWRIdDescriptionSeparator[] = "-";

std::string EntryKey(int64_t service_worker_registration_id,
                     const std::string& description_id) {
  return base::NumberToString(service_worker_registration_id) +
         kSWRIdDescriptionSeparator + description_id;
}

std::string EntryKey(const content::ContentIndexEntry& entry) {
  return EntryKey(entry.service_worker_registration_id, entry.description->id);
}

std::pair<int64_t, std::string> GetEntryKeyComponents(const std::string& key) {
  size_t pos = key.find_first_of(kSWRIdDescriptionSeparator);
  DCHECK_NE(pos, std::string::npos);

  int64_t service_worker_registration_id = -1;
  base::StringToInt64(base::StringPiece(key.data(), pos),
                      &service_worker_registration_id);

  return {service_worker_registration_id, key.substr(pos + 1)};
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

  // TODO(crbug.com/973844): Include URL info.

  return item;
}

}  // namespace

ContentIndexProviderImpl::ContentIndexProviderImpl(
    offline_items_collection::OfflineContentAggregator* aggregator)
    : aggregator_(aggregator), weak_ptr_factory_(this) {
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
    content::ContentIndexEntry entry,
    base::WeakPtr<content::ContentIndexProvider::Client> client) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string entry_key = EntryKey(entry);
  EntryData entry_data = {EntryToOfflineItem(entry), std::move(client)};

  bool is_update = entries_.erase(entry_key);

  if (is_update) {
    offline_items_collection::UpdateDelta delta;
    delta.visuals_changed = true;
    for (auto& observer : observers_)
      observer.OnItemUpdated(entry_data.offline_item, delta);
  } else {
    OfflineItemList items(1, entry_data.offline_item);
    for (auto& observer : observers_)
      observer.OnItemsAdded(items);
  }

  entries_.emplace(std::move(entry_key), std::move(entry_data));
}

void ContentIndexProviderImpl::OnContentDeleted(
    int64_t service_worker_registration_id,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto did_erase =
      entries_.erase(EntryKey(service_worker_registration_id, description_id));

  if (did_erase) {
    ContentId id(kProviderNamespace,
                 EntryKey(service_worker_registration_id, description_id));
    for (auto& observer : observers_)
      observer.OnItemRemoved(id);
  }
}

void ContentIndexProviderImpl::OpenItem(LaunchLocation location,
                                        const ContentId& id) {
  NOTIMPLEMENTED();
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
      FROM_HERE, base::BindOnce(std::move(callback), it->second.offline_item));
}

void ContentIndexProviderImpl::GetAllItems(MultipleItemCallback callback) {
  OfflineItemList list;
  for (const auto& entry : entries_)
    list.push_back(entry.second.offline_item);

  // TODO(crbug.com/1687257): Consider fetching these from the DB rather than
  // storing them in memory.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(list)));
}

void ContentIndexProviderImpl::GetVisualsForItem(const ContentId& id,
                                                 GetVisualsOptions options,
                                                 VisualsCallback callback) {
  auto it = entries_.find(id.id);
  if (it == entries_.end() || !it->second.client) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

  auto components = GetEntryKeyComponents(id.id);
  it->second.client->GetIcon(
      components.first, components.second,
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
