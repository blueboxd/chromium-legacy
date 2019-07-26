// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include <string>

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/content_index/content_index.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/973844): Move image utility functions to common library.
using content::background_fetch::DeserializeIcon;
using content::background_fetch::SerializeIcon;

namespace content {

namespace {

constexpr char kEntryPrefix[] = "content_index:entry_";
constexpr char kIconPrefix[] = "content_index:icon_";

std::string EntryKey(const std::string& id) {
  return kEntryPrefix + id;
}

std::string IconKey(const std::string& id) {
  return kIconPrefix + id;
}

std::string CreateSerializedContentEntry(
    const blink::mojom::ContentDescription& description,
    const GURL& launch_url,
    base::Time entry_time) {
  // Convert description.
  proto::ContentDescription description_proto;
  description_proto.set_id(description.id);
  description_proto.set_title(description.title);
  description_proto.set_description(description.description);
  description_proto.set_category(static_cast<int>(description.category));
  description_proto.set_icon_url(description.icon_url);
  description_proto.set_launch_url(description.launch_url);

  // Create entry.
  proto::ContentEntry entry;
  *entry.mutable_description() = std::move(description_proto);
  entry.set_launch_url(launch_url.spec());
  entry.set_timestamp(entry_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return entry.SerializeAsString();
}

blink::mojom::ContentDescriptionPtr DescriptionFromProto(
    const proto::ContentDescription& description) {
  // Validate.
  if (description.category() <
          static_cast<int>(blink::mojom::ContentCategory::kMinValue) ||
      description.category() >
          static_cast<int>(blink::mojom::ContentCategory::kMaxValue)) {
    return nullptr;
  }

  // Convert.
  auto result = blink::mojom::ContentDescription::New();
  result->id = description.id();
  result->title = description.title();
  result->description = description.description();
  result->category =
      static_cast<blink::mojom::ContentCategory>(description.category());
  result->icon_url = description.icon_url();
  result->launch_url = description.launch_url();
  return result;
}

base::Optional<ContentIndexEntry> EntryFromSerializedProto(
    int64_t service_worker_registration_id,
    const std::string& serialized_proto) {
  proto::ContentEntry entry_proto;
  if (!entry_proto.ParseFromString(serialized_proto))
    return base::nullopt;

  GURL launch_url(entry_proto.launch_url());
  if (!launch_url.is_valid())
    return base::nullopt;

  auto description = DescriptionFromProto(entry_proto.description());
  base::Time registration_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(entry_proto.timestamp()));

  return ContentIndexEntry(service_worker_registration_id,
                           std::move(description), std::move(launch_url),
                           registration_time);
}

}  // namespace

ContentIndexDatabase::ContentIndexDatabase(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : provider_(browser_context->GetContentIndexProvider()),
      service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_io_(this),
      weak_ptr_factory_ui_(this) {}

ContentIndexDatabase::~ContentIndexDatabase() = default;

void ContentIndexDatabase::AddEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::ContentDescriptionPtr description,
    const SkBitmap& icon,
    const GURL& launch_url,
    blink::mojom::ContentIndexService::AddCallback callback) {
  if (blocked_origins_.count(origin)) {
    // TODO(crbug.com/973844): Does this need a more specific error?
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  SerializeIcon(icon, base::BindOnce(&ContentIndexDatabase::DidSerializeIcon,
                                     weak_ptr_factory_io_.GetWeakPtr(),
                                     service_worker_registration_id, origin,
                                     std::move(description), launch_url,
                                     std::move(callback)));
}

void ContentIndexDatabase::DidSerializeIcon(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::ContentDescriptionPtr description,
    const GURL& launch_url,
    blink::mojom::ContentIndexService::AddCallback callback,
    std::string serialized_icon) {
  base::Time entry_time = base::Time::Now();
  std::string entry_key = EntryKey(description->id);
  std::string icon_key = IconKey(description->id);
  std::string entry_value =
      CreateSerializedContentEntry(*description, launch_url, entry_time);

  // Entry to pass over to the provider.
  ContentIndexEntry entry(service_worker_registration_id,
                          std::move(description), launch_url, entry_time);

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, origin.GetURL(),
      {{std::move(entry_key), std::move(entry_value)},
       {std::move(icon_key), std::move(serialized_icon)}},
      base::BindOnce(&ContentIndexDatabase::DidAddEntry,
                     weak_ptr_factory_io_.GetWeakPtr(), std::move(callback),
                     std::move(entry)));
}

void ContentIndexDatabase::DidAddEntry(
    blink::mojom::ContentIndexService::AddCallback callback,
    ContentIndexEntry entry,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);

  std::vector<ContentIndexEntry> entries;
  entries.push_back(std::move(entry));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ContentIndexDatabase::NotifyProviderContentAdded,
                     weak_ptr_factory_ui_.GetWeakPtr(), std::move(entries)));
}

void ContentIndexDatabase::DeleteEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback) {
  service_worker_context_->ClearRegistrationUserData(
      service_worker_registration_id, {EntryKey(entry_id), IconKey(entry_id)},
      base::BindOnce(&ContentIndexDatabase::DidDeleteEntry,
                     weak_ptr_factory_io_.GetWeakPtr(),
                     service_worker_registration_id, origin, entry_id,
                     std::move(callback)));
}

void ContentIndexDatabase::DidDeleteEntry(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
    return;
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ContentIndexDatabase::NotifyProviderContentDeleted,
                     weak_ptr_factory_ui_.GetWeakPtr(),
                     service_worker_registration_id, origin, entry_id));
}

void ContentIndexDatabase::GetDescriptions(
    int64_t service_worker_registration_id,
    blink::mojom::ContentIndexService::GetDescriptionsCallback callback) {
  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      service_worker_registration_id, kEntryPrefix,
      base::BindOnce(&ContentIndexDatabase::DidGetDescriptions,
                     weak_ptr_factory_io_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexDatabase::DidGetDescriptions(
    blink::mojom::ContentIndexService::GetDescriptionsCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                            /* descriptions= */ {});
    return;
  } else if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                            /* descriptions= */ {});
    return;
  }

  std::vector<blink::mojom::ContentDescriptionPtr> descriptions;
  descriptions.reserve(data.size());

  // TODO(crbug.com/973844): Clear the storage if there is data corruption.
  for (const auto& serialized_entry : data) {
    proto::ContentEntry entry;
    if (!entry.ParseFromString(serialized_entry)) {
      std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                              /* descriptions= */ {});
      return;
    }

    auto description = DescriptionFromProto(entry.description());
    if (!description) {
      std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                              /* descriptions= */ {});
      return;
    }

    descriptions.push_back(std::move(description));
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                          std::move(descriptions));
}

void ContentIndexDatabase::GetIcon(
    int64_t service_worker_registration_id,
    const std::string& description_id,
    base::OnceCallback<void(SkBitmap)> icon_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {IconKey(description_id)},
      base::BindOnce(&ContentIndexDatabase::DidGetSerializedIcon,
                     weak_ptr_factory_io_.GetWeakPtr(),
                     std::move(icon_callback)));
}

void ContentIndexDatabase::DidGetSerializedIcon(
    base::OnceCallback<void(SkBitmap)> icon_callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk || data.empty()) {
    std::move(icon_callback).Run(SkBitmap());
    return;
  }

  DCHECK_EQ(data.size(), 1u);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DeserializeIcon, std::make_unique<std::string>(data[0]),
                     std::move(icon_callback)));
}

void ContentIndexDatabase::GetAllEntries(
    ContentIndexContext::GetAllEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kEntryPrefix,
      base::BindOnce(&ContentIndexDatabase::DidGetEntries,
                     weak_ptr_factory_io_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexDatabase::DidGetEntries(
    ContentIndexContext::GetAllEntriesCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // TODO(crbug.com/973844): Handle or report this error.
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                            /* entries= */ {});
    return;
  }

  if (user_data.empty()) {
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                            /* entries= */ {});
    return;
  }

  std::vector<ContentIndexEntry> entries;
  entries.reserve(user_data.size());

  for (const auto& ud : user_data) {
    auto entry = EntryFromSerializedProto(ud.first, ud.second);
    if (!entry) {
      // TODO(crbug.com/973844): Handle or report this error.
      std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR,
                              /* entries= */ {});
      return;
    }

    entries.emplace_back(std::move(*entry));
  }

  std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                          std::move(entries));
}

void ContentIndexDatabase::GetEntry(
    int64_t service_worker_registration_id,
    const std::string& description_id,
    ContentIndexContext::GetEntryCallback callback) {
  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {EntryKey(description_id)},
      base::BindOnce(&ContentIndexDatabase::DidGetEntry,
                     weak_ptr_factory_io_.GetWeakPtr(),
                     service_worker_registration_id, std::move(callback)));
}

void ContentIndexDatabase::DidGetEntry(
    int64_t service_worker_registration_id,
    ContentIndexContext::GetEntryCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // TODO(crbug.com/973844): Handle or report this error.
    std::move(callback).Run(base::nullopt);
    return;
  }

  DCHECK_EQ(data.size(), 1u);
  std::move(callback).Run(
      EntryFromSerializedProto(service_worker_registration_id, data.front()));
}

void ContentIndexDatabase::BlockOrigin(const url::Origin& origin) {
  blocked_origins_[origin]++;
}

void ContentIndexDatabase::UnblockOrigin(const url::Origin& origin) {
  DCHECK(blocked_origins_.count(origin));
  auto it = blocked_origins_.find(origin);
  if (it->second == 1)
    blocked_origins_.erase(it);
  else
    it->second--;
}

void ContentIndexDatabase::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  provider_ = nullptr;
}

void ContentIndexDatabase::NotifyProviderContentAdded(
    std::vector<ContentIndexEntry> entries) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!provider_)
    return;

  for (auto& entry : entries)
    provider_->OnContentAdded(std::move(entry));
}

void ContentIndexDatabase::NotifyProviderContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& entry_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!provider_)
    return;

  provider_->OnContentDeleted(service_worker_registration_id, origin, entry_id);
}

}  // namespace content
