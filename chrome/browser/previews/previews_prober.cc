// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

std::string HttpMethodToString(PreviewsProber::HttpMethod http_method) {
  switch (http_method) {
    case PreviewsProber::HttpMethod::kGet:
      return "GET";
    case PreviewsProber::HttpMethod::kHead:
      return "HEAD";
  }
}

}  // namespace

PreviewsProber::PreviewsProber(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& name,
    const GURL& url,
    const HttpMethod http_method)
    : name_(name),
      url_(url),
      http_method_(http_method),
      is_active_(false),
      last_probe_status_(base::nullopt),
      url_loader_factory_(url_loader_factory) {}

PreviewsProber::~PreviewsProber() = default;

void PreviewsProber::SendNowIfInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_active_)
    return;
  CreateAndStartURLLoader();
}

void PreviewsProber::CreateAndStartURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_active_);
  DCHECK(!url_loader_);

  is_active_ = true;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("previews_prober", R"(
        semantics {
          sender: "Previews Prober"
          description:
            "Requests a small resource to test network connectivity to a given "
            "resource or domain which will either be a Google owned domain or"
            "the website that the user is navigating to."
          trigger:
            "Requested when Lite mode and Previews are enabled on startup and "
            "on every network change."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via the settings menu. "
            "Lite mode is not available on iOS, and on desktop only for "
            "developer testing."
          policy_exception_justification: "Not implemented."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->method = HttpMethodToString(http_method_);
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->allow_credentials = false;

  // TODO(crbug/977603): Set retry options.
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PreviewsProber::OnURLLoadComplete,
                     base::Unretained(this)),
      1024);
}

void PreviewsProber::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  // TODO(crbug/977603): Replace with delegate check.
  last_probe_status_ =
      url_loader_->NetError() == net::OK && response_code == net::HTTP_OK;

  url_loader_.reset();
  is_active_ = false;
}

base::Optional<bool> PreviewsProber::LastProbeWasSuccessful() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_probe_status_;
}
