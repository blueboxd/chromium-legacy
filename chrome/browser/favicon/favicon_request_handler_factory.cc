// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_request_handler_factory.h"

#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_request_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
favicon::FaviconRequestHandler*
FaviconRequestHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<favicon::FaviconRequestHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FaviconRequestHandlerFactory* FaviconRequestHandlerFactory::GetInstance() {
  return base::Singleton<FaviconRequestHandlerFactory>::get();
}

FaviconRequestHandlerFactory::FaviconRequestHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "FaviconRequestHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(LargeIconServiceFactory::GetInstance());
}

FaviconRequestHandlerFactory::~FaviconRequestHandlerFactory() {}

content::BrowserContext* FaviconRequestHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* FaviconRequestHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new favicon::FaviconRequestHandler(
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      LargeIconServiceFactory::GetForBrowserContext(context));
}

bool FaviconRequestHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
