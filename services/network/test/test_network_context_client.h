// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

// A helper class with a basic NetworkContextClient implementation for use in
// unittests, so they can just override the parts they need.
class TestNetworkContextClient : public network::mojom::NetworkContextClient {
 public:
  TestNetworkContextClient() = default;
  ~TestNetworkContextClient() override = default;

  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override {}
  void OnCanSendDomainReliabilityUpload(
      const GURL& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override {}
  void OnClearSiteData(uint32_t process_id,
                       int32_t routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int32_t load_flags,
                       OnClearSiteDataCallback callback) override {}
  void OnCookiesChanged(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {}
  void OnCookiesRead(
      bool is_service_worker,
      int32_t process_id,
      int32_t routing_id,
      const GURL& url,
      const GURL& site_for_cookies,
      const std::vector<net::CookieWithStatus>& cookie_list) override {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONTEXT_CLIENT_H_
