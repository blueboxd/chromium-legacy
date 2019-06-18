// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

#include "base/guid.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace content {

TEST(ServiceWorkerRequestTest, SerialiazeDeserializeRoundTrip) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->mode = network::mojom::RequestMode::kSameOrigin;
  request->is_main_resource_load = true;
  request->request_context_type = blink::mojom::RequestContextType::IFRAME;
  request->url = GURL("foo.com");
  request->method = "GET";
  request->headers = {{"User-Agent", "Chrome"}};
  request->referrer = blink::mojom::Referrer::New(
      GURL("bar.com"),
      network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade);
  request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  request->cache_mode = blink::mojom::FetchCacheMode::kForceCache;
  request->redirect_mode = network::mojom::RedirectMode::kManual;
  request->integrity = "integrity";
  request->keepalive = true;
  request->is_reload = true;

  EXPECT_EQ(
      ServiceWorkerUtils::SerializeFetchRequestToString(*request),
      ServiceWorkerUtils::SerializeFetchRequestToString(
          *ServiceWorkerUtils::DeserializeFetchRequestFromString(
              ServiceWorkerUtils::SerializeFetchRequestToString(*request))));
}

}  // namespace content
