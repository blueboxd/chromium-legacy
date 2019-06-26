// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include <string>

#include "base/time/time.h"
#include "content/browser/content_index/content_index.pb.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kEntryPrefix[] = "content_index:entry_";

std::string EntryKey(const std::string& id) {
  return kEntryPrefix + id;
}

std::string CreateSerializedContentEntry(
    blink::mojom::ContentDescriptionPtr description) {
  // Convert description.
  proto::ContentDescription description_proto;
  description_proto.set_id(std::move(description->id));
  description_proto.set_title(std::move(description->title));
  description_proto.set_description(std::move(description->description));
  description_proto.set_category(static_cast<int>(description->category));
  description_proto.set_icon_url(description->icon_url.spec());
  description_proto.set_launch_url(description->launch_url.spec());

  // Create entry.
  proto::ContentEntry entry;
  *entry.mutable_description() = std::move(description_proto);
  entry.set_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

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
  result->icon_url = GURL(description.icon_url());
  result->launch_url = GURL(description.launch_url());
  return result;
}

}  // namespace

ContentIndexDatabase::ContentIndexDatabase(
    const url::Origin& origin,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : origin_(origin),
      service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {}

ContentIndexDatabase::~ContentIndexDatabase() = default;

void ContentIndexDatabase::AddEntry(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const SkBitmap& icon,
    blink::mojom::ContentIndexService::AddCallback callback) {
  std::string key = EntryKey(description->id);
  std::string value = CreateSerializedContentEntry(std::move(description));
  DCHECK(!value.empty());

  // TODO(crbug.com/973844): Serialize and store icon.
  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, origin_.GetURL(),
      {{std::move(key), std::move(value)}},
      base::BindOnce(&ContentIndexDatabase::DidAddEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexDatabase::DidAddEntry(
    blink::mojom::ContentIndexService::AddCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk)
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
  else
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE);
}

void ContentIndexDatabase::DeleteEntry(
    int64_t service_worker_registration_id,
    const std::string& entry_id,
    blink::mojom::ContentIndexService::DeleteCallback callback) {
  service_worker_context_->ClearRegistrationUserData(
      service_worker_registration_id, {EntryKey(entry_id)},
      base::BindOnce(&ContentIndexDatabase::DidDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentIndexDatabase::DidDeleteEntry(
    blink::mojom::ContentIndexService::DeleteCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk)
    std::move(callback).Run(blink::mojom::ContentIndexError::STORAGE_ERROR);
  else
    std::move(callback).Run(blink::mojom::ContentIndexError::NONE);
}

void ContentIndexDatabase::GetDescriptions(
    int64_t service_worker_registration_id,
    blink::mojom::ContentIndexService::GetDescriptionsCallback callback) {
  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      service_worker_registration_id, kEntryPrefix,
      base::BindOnce(&ContentIndexDatabase::DidGetDescriptions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

}  // namespace content
