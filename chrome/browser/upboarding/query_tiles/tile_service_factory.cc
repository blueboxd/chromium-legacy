// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/tile_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/upboarding/query_tiles/tile_service_factory_helper.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

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
  return CreateTileService(image_fetcher_service);
}

}  // namespace upboarding
