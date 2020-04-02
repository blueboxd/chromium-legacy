// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppMetrics;

// A factory to create WebAppMetrics. Its KeyedService shouldn't be created with
// browser context (forces the creation of SiteEngagementService otherwise).
class WebAppMetricsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebAppMetrics* GetForProfile(Profile* profile);

  static WebAppMetricsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebAppMetricsFactory>;

  WebAppMetricsFactory();
  ~WebAppMetricsFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppMetricsFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_FACTORY_H_
