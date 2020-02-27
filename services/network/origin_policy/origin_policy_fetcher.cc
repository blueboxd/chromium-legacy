// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/origin_policy/origin_policy_fetcher.h"

#include <utility>

#include "base/strings/strcat.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/origin_policy/origin_policy_parser.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

OriginPolicyFetcher::OriginPolicyFetcher(
    OriginPolicyManager* owner_policy_manager,
    const url::Origin& origin,
    mojom::URLLoaderFactory* factory,
    mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback)
    : owner_policy_manager_(owner_policy_manager),
      fetch_url_(GetPolicyURL(origin)),
      callback_(std::move(callback)) {
  DCHECK(callback_);
  FetchPolicy(factory);
}

OriginPolicyFetcher::~OriginPolicyFetcher() = default;

// static
GURL OriginPolicyFetcher::GetPolicyURL(const url::Origin& origin) {
  return GURL(base::StrCat({origin.Serialize(), kOriginPolicyWellKnown}));
}

void OriginPolicyFetcher::FetchPolicy(mojom::URLLoaderFactory* factory) {
  // Create the traffic annotation
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("origin_policy_loader", R"(
        semantics {
          sender: "Origin Policy URL Loader Throttle"
          description:
            "Fetches the Origin Policy from an origin."
          trigger:
            "The server has used the Origin-Policy header to request that an "
            "origin policy be applied."
          data:
            "None; the URL itself contains the origin."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings. Servers "
            "opt in or out of this mechanism."
          policy_exception_justification:
            "Not implemented, considered not useful."})");

  FetchCallback done = base::BindOnce(&OriginPolicyFetcher::OnPolicyHasArrived,
                                      base::Unretained(this));

  // Create and configure the SimpleURLLoader for the policy.
  std::unique_ptr<ResourceRequest> policy_request =
      std::make_unique<ResourceRequest>();
  policy_request->url = fetch_url_;
  policy_request->request_initiator = url::Origin::Create(fetch_url_);
  policy_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  policy_request->redirect_mode = network::mojom::RedirectMode::kError;

  url_loader_ =
      SimpleURLLoader::Create(std::move(policy_request), traffic_annotation);

  // Start the download, and pass the callback for when we're finished.
  url_loader_->DownloadToString(factory, std::move(done),
                                kOriginPolicyMaxPolicySize);
}

void OriginPolicyFetcher::OnPolicyHasArrived(
    std::unique_ptr<std::string> policy_content) {
  OriginPolicy result;
  result.state = policy_content ? OriginPolicyState::kLoaded
                                : OriginPolicyState::kCannotLoadPolicy;

  if (policy_content)
    result.contents = OriginPolicyParser::Parse(*policy_content);

  result.policy_url = fetch_url_;

  // Do not add code after this call as it will destroy this object.
  owner_policy_manager_->FetcherDone(this, result, std::move(callback_));
}

}  // namespace network
