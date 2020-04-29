// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_constants.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/query_tiles/tile_service_factory_helper.h"

namespace upboarding {

// static
TileServiceFactory* TileServiceFactory::GetInstance() {
  return base::Singleton<TileServiceFactory>::get();
}

// static
TileService* TileServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<TileService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

TileServiceFactory::TileServiceFactory()
    : SimpleKeyedServiceFactory("TileService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(ImageFetcherServiceFactory::GetInstance());
}

TileServiceFactory::~TileServiceFactory() {}

std::unique_ptr<KeyedService> TileServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto* image_fetcher_service = ImageFetcherServiceFactory::GetForKey(key);
  auto* db_provider =
      ProfileKey::FromSimpleFactoryKey(key)->GetProtoDatabaseProvider();
  // |storage_dir| is not actually used since we are using the shared leveldb.
  base::FilePath storage_dir =
      ProfileKey::FromSimpleFactoryKey(key)->GetPath().Append(
          chrome::kQueryTileStorageDirname);
  return CreateTileService(image_fetcher_service, db_provider, storage_dir);
}

}  // namespace upboarding
