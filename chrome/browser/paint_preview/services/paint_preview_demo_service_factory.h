// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/paint_preview/services/paint_preview_demo_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace paint_preview {

class PaintPreviewDemoServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static PaintPreviewDemoServiceFactory* GetInstance();
  static PaintPreviewDemoService* GetForKey(SimpleFactoryKey* key);

  PaintPreviewDemoServiceFactory(const PaintPreviewDemoServiceFactory&) =
      delete;
  PaintPreviewDemoServiceFactory& operator=(
      const PaintPreviewDemoServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PaintPreviewDemoServiceFactory>;

  PaintPreviewDemoServiceFactory();

  ~PaintPreviewDemoServiceFactory() override;

  // SimpleKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;

  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_DEMO_SERVICE_FACTORY_H_
