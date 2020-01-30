// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/captive_portal_service_factory.h"

#include "components/captive_portal/content/captive_portal_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
CaptivePortalService* CaptivePortalServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<CaptivePortalService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
CaptivePortalServiceFactory* CaptivePortalServiceFactory::GetInstance() {
  return base::Singleton<CaptivePortalServiceFactory>::get();
}

CaptivePortalServiceFactory::CaptivePortalServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CaptivePortalService",
          BrowserContextDependencyManager::GetInstance()) {}

CaptivePortalServiceFactory::~CaptivePortalServiceFactory() = default;

KeyedService* CaptivePortalServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new CaptivePortalService(browser_context,
                                  user_prefs::UserPrefs::Get(browser_context));
}

content::BrowserContext* CaptivePortalServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
