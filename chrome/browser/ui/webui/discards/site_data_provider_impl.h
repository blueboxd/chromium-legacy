// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace resource_coordinator {
class LocalSiteCharacteristicsDataStoreInspector;
class SiteCharacteristicsDataReader;
}  // namespace resource_coordinator

class SiteDataProviderImpl : public discards::mojom::SiteDataProvider {
 public:
  SiteDataProviderImpl(
      resource_coordinator::LocalSiteCharacteristicsDataStoreInspector*
          data_store_inspector,
      mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver);
  ~SiteDataProviderImpl() override;
  SiteDataProviderImpl(const SiteDataProviderImpl& other) = delete;
  SiteDataProviderImpl& operator=(const SiteDataProviderImpl&) = delete;

  void GetSiteCharacteristicsDatabase(
      const std::vector<std::string>& explicitly_requested_origins,
      GetSiteCharacteristicsDatabaseCallback callback) override;
  void GetSiteCharacteristicsDatabaseSize(
      GetSiteCharacteristicsDatabaseSizeCallback callback) override;

 private:
  using LocalSiteCharacteristicsDataStoreInspector =
      resource_coordinator::LocalSiteCharacteristicsDataStoreInspector;
  using SiteCharacteristicsDataReader =
      resource_coordinator::SiteCharacteristicsDataReader;
  using OriginToReaderMap =
      std::map<std::string, std::unique_ptr<SiteCharacteristicsDataReader>>;

  // This map pins requested readers and their associated data in memory until
  // after the next read finishes. This is necessary to allow the database reads
  // to go through and populate the requested entries.
  OriginToReaderMap requested_origins_;

  resource_coordinator::LocalSiteCharacteristicsDataStoreInspector*
      data_store_inspector_ = nullptr;

  mojo::Receiver<discards::mojom::SiteDataProvider> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_
