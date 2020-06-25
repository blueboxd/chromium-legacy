// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "components/safe_browsing/core/realtime/url_lookup_service_base.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class DMToken;
}  // namespace policy

class Profile;

namespace safe_browsing {

// This class implements the real time lookup feature for a given user/profile.
// It is separated from the base class for logic that is related to enterprise
// users.(See: go/chrome-protego-enterprise-dd)
class ChromeEnterpriseRealTimeUrlLookupService
    : public RealTimeUrlLookupServiceBase {
 public:
  ChromeEnterpriseRealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      Profile* profile);
  ~ChromeEnterpriseRealTimeUrlLookupService() override;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;

  bool CanCheckSubresourceURL() const override;

  bool CanCheckSafeBrowsingDb() const override;

  void StartLookup(const GURL& url,
                   RTLookupRequestCallback request_callback,
                   RTLookupResponseCallback response_callback) override;

 private:
  // RealTimeUrlLookupServiceBase:
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;

  GURL GetRealTimeLookupUrl() const override;

  policy::DMToken GetDMToken() const;

  // Unowned object used for checking profile based settings.
  Profile* profile_;

  base::WeakPtrFactory<ChromeEnterpriseRealTimeUrlLookupService> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeEnterpriseRealTimeUrlLookupService);

};  // class ChromeEnterpriseRealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
