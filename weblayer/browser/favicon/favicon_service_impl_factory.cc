// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_service_impl_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

// static
FaviconServiceImpl* FaviconServiceImplFactory::GetForProfile(
    ProfileImpl* profile) {
  if (!profile->GetBrowserContext()->IsOffTheRecord()) {
    return static_cast<FaviconServiceImpl*>(
        GetInstance()->GetServiceForBrowserContext(profile->GetBrowserContext(),
                                                   true));
  }
  return nullptr;
}

// static
FaviconServiceImplFactory* FaviconServiceImplFactory::GetInstance() {
  static base::NoDestructor<FaviconServiceImplFactory> factory;
  return factory.get();
}

FaviconServiceImplFactory::FaviconServiceImplFactory()
    : BrowserContextKeyedServiceFactory(
          "FaviconServiceImpl",
          BrowserContextDependencyManager::GetInstance()) {}

FaviconServiceImplFactory::~FaviconServiceImplFactory() = default;

KeyedService* FaviconServiceImplFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(sky): figure out best way to support incognito.
  DCHECK(!context->IsOffTheRecord());
  std::unique_ptr<FaviconServiceImpl> service =
      std::make_unique<FaviconServiceImpl>();
  service->Init(static_cast<BrowserContextImpl*>(context)
                    ->profile_impl()
                    ->data_path()
                    .AppendASCII("Favicons"));
  return service.release();
}

bool FaviconServiceImplFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace weblayer
