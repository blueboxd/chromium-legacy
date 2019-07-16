// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

// A class to handle requests for optimization hints from a remote Optimization
// Guide Service.
//
// This class fetches new hints from the remote Optimization Guide Service.
// Owner must ensure that |hint_cache| remains alive for the lifetime of
// |HintsFetcher|.
class HintsFetcher {
  // Callback to inform the caller that the remote hints have been fetched and
  // to pass back the fetched hints response from the remote Optimization Guide
  // Service.
  using HintsFetchedCallback = base::OnceCallback<void(
      base::Optional<std::unique_ptr<proto::GetHintsResponse>>)>;

 public:
  HintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url);
  virtual ~HintsFetcher();

  // Requests hints from the Optimization Guide Service if a request for them is
  // not already in progress. Returns whether a new request was issued.
  // |hints_fetched_callback| is run, passing a GetHintsResponse object, if a
  // fetch was successful or passes nullopt if the fetch fails. Virtualized for
  // testing.
  virtual bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      HintsFetchedCallback hints_fetched_callback);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);

  // Used to hold the GetHintsRequest being constructed and sent as a remote
  // request.
  std::unique_ptr<proto::GetHintsRequest> get_hints_request_;

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  HintsFetchedCallback hints_fetched_callback_;

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // Holds the |URLLoader| for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for creating a |url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintsFetcher);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_
