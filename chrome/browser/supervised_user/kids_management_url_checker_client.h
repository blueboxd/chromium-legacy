// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/safe_search_api/url_checker_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
struct AccessTokenInfo;
}  // namespace signin

struct KidsManagementURLCheckerResponse;

// This class uses the KidsManagement ClassifyUrl to check the classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class KidsManagementURLCheckerClient
    : public safe_search_api::URLCheckerClient {
 public:
  // |country| should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us".
  KidsManagementURLCheckerClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& country,
      signin::IdentityManager* identity_manager);
  ~KidsManagementURLCheckerClient() override;

  // Checks whether an |url| is restricted according to KidsManagement
  // ClassifyUrl RPC.
  //
  // On failure, the |callback| function is called with |url| as the first
  // parameter, and UNKNOWN as the second.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  struct Check;

  // Must be list so that iterators stay valid until they are erased.
  using CheckList = std::list<std::unique_ptr<Check>>;

  void StartFetching(CheckList::iterator it);

  void OnAccessTokenFetchComplete(CheckList::iterator it,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(CheckList::iterator it,
                              signin::AccessTokenInfo token_info,
                              std::unique_ptr<std::string> response_body);

  void DispatchResult(CheckList::iterator it,
                      KidsManagementURLCheckerResponse response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  const std::string country_;
  signin::IdentityManager* identity_manager_;

  CheckList checks_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(KidsManagementURLCheckerClient);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
