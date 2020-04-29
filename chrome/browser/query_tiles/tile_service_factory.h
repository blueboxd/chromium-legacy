// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/query_tiles/tile_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace upboarding {

class TileService;

// A factory to create one unique TileService.
class TileServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static TileServiceFactory* GetInstance();
  static TileService* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<TileServiceFactory>;

  TileServiceFactory();
  ~TileServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(TileServiceFactory);
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_QUERY_TILES_TILE_SERVICE_FACTORY_H_
