// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/cors/preflight_cache.h"
#include "services/network/public/cpp/cors/preflight_result.h"
#include "services/network/public/cpp/cors/preflight_timing_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace network {

namespace cors {

// A class to manage CORS-preflight, making a CORS-preflight request, checking
// its result, and owning a CORS-preflight cache.
// TODO(toyoshim): Features explained above not fully implemented yet.
// See also https://crbug.com/803766 to check a design doc.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightController final {
 public:
  // PreflightTimingInfo is provided only when a preflight request was made.
  using CompletionCallback =
      base::OnceCallback<void(int net_error,
                              base::Optional<CorsErrorStatus>,
                              base::Optional<PreflightTimingInfo>)>;
  // Creates a CORS-preflight ResourceRequest for a specified |request| for a
  // URL that is originally requested.
  static std::unique_ptr<ResourceRequest> CreatePreflightRequestForTesting(
      const ResourceRequest& request,
      bool tainted = false);
  // Creates a PreflightResult for a specified response parameters for testing.
  static std::unique_ptr<PreflightResult> CreatePreflightResultForTesting(
      const GURL& final_url,
      const ResourceResponseHead& head,
      const ResourceRequest& original_request,
      bool tainted,
      base::Optional<CorsErrorStatus>* detected_error_status);

  PreflightController();
  ~PreflightController();

  // Determines if a CORS-preflight request is needed, and checks the cache, or
  // makes a preflight request if it is needed. A result will be notified
  // synchronously or asynchronously.
  void PerformPreflightCheck(
      CompletionCallback callback,
      const ResourceRequest& resource_request,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* loader_factory);

 private:
  class PreflightLoader;

  void RemoveLoader(PreflightLoader* loader);
  void AppendToCache(const url::Origin& origin,
                     const GURL& url,
                     std::unique_ptr<PreflightResult> result);

  PreflightCache cache_;
  std::set<std::unique_ptr<PreflightLoader>, base::UniquePtrComparator>
      loaders_;

  DISALLOW_COPY_AND_ASSIGN(PreflightController);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
