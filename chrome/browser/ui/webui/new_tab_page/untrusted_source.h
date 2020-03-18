// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_

#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_observer.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_observer.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves chrome-untrusted://new-tab-page/* sources which can return content
// from outside the chromium codebase. The chrome-untrusted://new-tab-page/*
// sources can only be embedded in the chrome://new-tab-page by using an
// <iframe>.
class UntrustedSource : public content::URLDataSource,
                        public OneGoogleBarServiceObserver,
                        public PromoServiceObserver {
 public:
  explicit UntrustedSource(Profile* profile);
  ~UntrustedSource() override;
  UntrustedSource(const UntrustedSource&) = delete;
  UntrustedSource& operator=(const UntrustedSource&) = delete;

  // content::URLDataSource:
  std::string GetContentSecurityPolicyScriptSrc() override;
  std::string GetContentSecurityPolicyChildSrc() override;
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;
  std::string GetContentSecurityPolicyFrameAncestors() override;
  bool ShouldReplaceExistingSource() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::ResourceContext* resource_context,
                            int render_process_id) override;

 private:
  // OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  // PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  std::vector<content::URLDataSource::GotDataCallback>
      one_google_bar_callbacks_;
  OneGoogleBarService* one_google_bar_service_;
  ScopedObserver<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observer_{this};
  std::vector<content::URLDataSource::GotDataCallback> promo_callbacks_;
  PromoService* promo_service_;
  ScopedObserver<PromoService, PromoServiceObserver> promo_service_observer_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
