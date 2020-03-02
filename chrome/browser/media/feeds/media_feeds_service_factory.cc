// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service_factory.h"

#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace media_feeds {

// static
MediaFeedsService* MediaFeedsServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  if (!MediaFeedsService::IsEnabled())
    return nullptr;

  return static_cast<MediaFeedsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaFeedsServiceFactory* MediaFeedsServiceFactory::GetInstance() {
  static base::NoDestructor<MediaFeedsServiceFactory> factory;
  return factory.get();
}

MediaFeedsServiceFactory::MediaFeedsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaFeedsService",
          BrowserContextDependencyManager::GetInstance()) {}

MediaFeedsServiceFactory::~MediaFeedsServiceFactory() = default;

bool MediaFeedsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* MediaFeedsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  if (!MediaFeedsService::IsEnabled())
    return nullptr;

  return new MediaFeedsService(Profile::FromBrowserContext(context));
}

}  // namespace media_feeds
