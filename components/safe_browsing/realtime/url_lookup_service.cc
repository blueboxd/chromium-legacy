// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/url_lookup_service.h"

#include "base/base64url.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

// TODO(vakh): Use the correct endpoint.
const char kRealTimeLookupUrlPrefix[] =
    "http://localhost:8000/safebrowsing/clientreport/realtime";

}  // namespace

RealTimeUrlLookupService::RealTimeUrlLookupService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

void RealTimeUrlLookupService::StartLookup(const GURL& url,
                                           RTLookupResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(url.is_valid());

  RTLookupRequest request;
  request.set_url(url.spec());
  request.set_lookup_type(RTLookupRequest::NAVIGATION);
  std::string req_data, req_base64;
  request.SerializeToString(&req_data);
  base::Base64UrlEncode(req_data, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &req_base64);
  // TODO(vakh): Add the correct chrome_policy field below.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_realtime_url_lookup",
                                          R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data: "The main frame URL that did not match the local safelist."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing real time URL checks by "
            "unchecking 'Protect you and your device from dangerous sites' in "
            "Chromium settings under Privacy, or by unchecking 'Make searches "
            "and browsing better (Sends URLs of pages you visit to Google)' in "
            "Chromium settings under Privacy."
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kRealTimeLookupUrlPrefix);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";

  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* loader = owned_loader.get();
  owned_loader->AttachStringForUpload(req_data, "application/octet-stream");
  owned_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupService::OnURLLoaderComplete,
                     base::Unretained(this), loader));

  // TODO(crbug.com/992099): Implement timeout and backoff.

  pending_requests_[owned_loader.release()] = std::move(callback);
}

RealTimeUrlLookupService::~RealTimeUrlLookupService() {
  for (auto& pending : pending_requests_) {
    // An empty response is treated as safe.
    auto response = std::make_unique<RTLookupResponse>();
    std::move(pending.second).Run(std::move(response));
    delete pending.first;
  }
  pending_requests_.clear();
}

void RealTimeUrlLookupService::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto it = pending_requests_.find(url_loader);
  DCHECK(it != pending_requests_.end()) << "Request not found";

  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
      "SafeBrowsing.RT.Network.Result", net_error, response_code);

  auto response = std::make_unique<RTLookupResponse>();
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    response->ParseFromString(*response_body);
  } else {
    HandleResponseError();
  }

  std::move(it->second).Run(std::move(response));
  delete it->first;
  pending_requests_.erase(it);
}

void RealTimeUrlLookupService::HandleResponseError() {
  // TODO(crbug.com/992099): Implement a backoff mechanism.
}

bool RealTimeUrlLookupService::IsInBackoffMode() {
  // TODO(crbug.com/992099): Implement a backoff mechanism.
  return false;
}

}  // namespace safe_browsing
