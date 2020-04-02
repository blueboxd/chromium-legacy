// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/site_data_provider_impl.h"

#include "base/bind_helpers.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"

namespace {

discards::mojom::SiteCharacteristicsFeaturePtr ConvertFeatureFromProto(
    const SiteDataFeatureProto& proto) {
  discards::mojom::SiteCharacteristicsFeaturePtr feature =
      discards::mojom::SiteCharacteristicsFeature::New();

  if (proto.has_observation_duration())
    feature->observation_duration = proto.observation_duration();

  if (proto.has_use_timestamp())
    feature->use_timestamp = proto.use_timestamp();

  return feature;
}

discards::mojom::SiteCharacteristicsDatabaseEntryPtr ConvertEntryFromProto(
    SiteDataProto* proto) {
  discards::mojom::SiteCharacteristicsDatabaseValuePtr value =
      discards::mojom::SiteCharacteristicsDatabaseValue::New();

  if (proto->has_last_loaded())
    value->last_loaded = proto->last_loaded();

  value->updates_favicon_in_background =
      ConvertFeatureFromProto(proto->updates_favicon_in_background());
  value->updates_title_in_background =
      ConvertFeatureFromProto(proto->updates_title_in_background());
  value->uses_audio_in_background =
      ConvertFeatureFromProto(proto->uses_audio_in_background());

  if (proto->has_load_time_estimates()) {
    const auto& load_time_estimates_proto = proto->load_time_estimates();
    DCHECK(load_time_estimates_proto.has_avg_cpu_usage_us());
    DCHECK(load_time_estimates_proto.has_avg_footprint_kb());

    discards::mojom::SiteCharacteristicsPerformanceMeasurementPtr
        load_time_estimates =
            discards::mojom::SiteCharacteristicsPerformanceMeasurement::New();
    if (load_time_estimates_proto.has_avg_cpu_usage_us()) {
      load_time_estimates->avg_cpu_usage_us =
          load_time_estimates_proto.avg_cpu_usage_us();
    }
    if (load_time_estimates_proto.has_avg_footprint_kb()) {
      load_time_estimates->avg_footprint_kb =
          load_time_estimates_proto.avg_footprint_kb();
    }
    if (load_time_estimates_proto.has_avg_load_duration_us()) {
      load_time_estimates->avg_load_duration_us =
          load_time_estimates_proto.avg_load_duration_us();
    }

    value->load_time_estimates = std::move(load_time_estimates);
  }

  discards::mojom::SiteCharacteristicsDatabaseEntryPtr entry =
      discards::mojom::SiteCharacteristicsDatabaseEntry::New();
  entry->value = std::move(value);
  return entry;
}

}  // namespace

SiteDataProviderImpl::SiteDataProviderImpl(
    resource_coordinator::LocalSiteCharacteristicsDataStoreInspector*
        data_store_inspector,
    mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver)
    : data_store_inspector_(data_store_inspector),
      receiver_(this, std::move(receiver)) {}

SiteDataProviderImpl::~SiteDataProviderImpl() = default;

void SiteDataProviderImpl::GetSiteCharacteristicsDatabase(
    const std::vector<std::string>& explicitly_requested_origins,
    GetSiteCharacteristicsDatabaseCallback callback) {
  // Move all previously explicitly requested origins to this local map.
  // Move any currently requested origins over to the member variable, or
  // populate them if they weren't previously requested.
  // The difference will remain in this map and go out of scope at the end of
  // this function.
  OriginToReaderMap prev_requested_origins;
  prev_requested_origins.swap(requested_origins_);
  resource_coordinator::SiteCharacteristicsDataStore* data_store =
      data_store_inspector_->GetDataStore();
  DCHECK(data_store);
  for (const std::string& origin : explicitly_requested_origins) {
    auto it = prev_requested_origins.find(origin);
    if (it == prev_requested_origins.end()) {
      GURL url(origin);
      requested_origins_[origin] =
          data_store->GetReaderForOrigin(url::Origin::Create(url));
    } else {
      requested_origins_[origin] = std::move(it->second);
    }
  }

  discards::mojom::SiteCharacteristicsDatabasePtr result =
      discards::mojom::SiteCharacteristicsDatabase::New();
  std::vector<url::Origin> in_memory_origins =
      data_store_inspector_->GetAllInMemoryOrigins();
  for (const url::Origin& origin : in_memory_origins) {
    // Get the data for this origin and convert it from proto to the
    // corresponding mojo structure.
    std::unique_ptr<SiteDataProto> proto;
    bool is_dirty = false;
    if (data_store_inspector_->GetDataForOrigin(origin, &is_dirty, &proto)) {
      auto entry = ConvertEntryFromProto(proto.get());
      entry->origin = origin.Serialize();
      entry->is_dirty = is_dirty;
      result->db_rows.push_back(std::move(entry));
    }
  }

  // Return the result.
  std::move(callback).Run(std::move(result));
}

void SiteDataProviderImpl::GetSiteCharacteristicsDatabaseSize(
    GetSiteCharacteristicsDatabaseSizeCallback callback) {
  // Adapt the inspector callback to the mojom callback with this lambda.
  auto inspector_callback = base::BindOnce(
      [](GetSiteCharacteristicsDatabaseSizeCallback callback,
         base::Optional<int64_t> num_rows,
         base::Optional<int64_t> on_disk_size_kb) {
        discards::mojom::SiteCharacteristicsDatabaseSizePtr result =
            discards::mojom::SiteCharacteristicsDatabaseSize::New();
        result->num_rows = num_rows.has_value() ? num_rows.value() : -1;
        result->on_disk_size_kb =
            on_disk_size_kb.has_value() ? on_disk_size_kb.value() : -1;

        std::move(callback).Run(std::move(result));
      },
      std::move(callback));

  data_store_inspector_->GetDatabaseSize(std::move(inspector_callback));
}
