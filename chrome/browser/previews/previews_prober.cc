// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include <math.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/time/default_tick_clock.h"
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

// Computes the time delta for a given Backoff algorithm, a base interval, and
// the count of how many attempts have been made thus far.
base::TimeDelta ComputeNextTimeDeltaForBackoff(PreviewsProber::Backoff backoff,
                                               base::TimeDelta base_interval,
                                               size_t attempts_so_far) {
  switch (backoff) {
    case PreviewsProber::Backoff::kLinear:
      return base_interval;
    case PreviewsProber::Backoff::kExponential:
      return base_interval * pow(2, attempts_so_far);
  }
}

}  // namespace

PreviewsProber::RetryPolicy::RetryPolicy() = default;
PreviewsProber::RetryPolicy::~RetryPolicy() = default;
PreviewsProber::RetryPolicy::RetryPolicy(PreviewsProber::RetryPolicy const&) =
    default;

PreviewsProber::PreviewsProber(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& name,
    const GURL& url,
    const HttpMethod http_method,
    const RetryPolicy& retry_policy)
    : PreviewsProber(url_loader_factory,
                     name,
                     url,
                     http_method,
                     retry_policy,
                     base::DefaultTickClock::GetInstance()) {}

PreviewsProber::PreviewsProber(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& name,
    const GURL& url,
    const HttpMethod http_method,
    const RetryPolicy& retry_policy,
    const base::TickClock* tick_clock)
    : name_(name),
      url_(url),
      http_method_(http_method),
      retry_policy_(retry_policy),
      successive_retry_count_(0),
      retry_timer_(nullptr),
      tick_clock_(tick_clock),
      is_active_(false),
      last_probe_status_(base::nullopt),
      url_loader_factory_(url_loader_factory) {}

PreviewsProber::~PreviewsProber() = default;

void PreviewsProber::SendNowIfInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_active_)
    return;

  successive_retry_count_ = 0;
  CreateAndStartURLLoader();
}

void PreviewsProber::CreateAndStartURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_active_ || successive_retry_count_ > 0);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!url_loader_);

  is_active_ = true;

  GURL url = url_;
  if (retry_policy_.use_random_urls) {
    std::string query = "guid=" + base::GenerateGUID();
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    url = url.ReplaceComponents(replacements);
  }

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
  request->url = url;
  request->method = HttpMethodToString(http_method_);
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->allow_credentials = false;

  // TODO(crbug/977603): Set retry options.
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetAllowHttpErrorResults(true);

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
  bool was_successful =
      url_loader_->NetError() == net::OK && response_code == net::HTTP_OK;

  url_loader_.reset();

  if (was_successful) {
    ProcessProbeSuccess();
    return;
  }
  ProcessProbeFailure();
}

void PreviewsProber::ProcessProbeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(is_active_);

  last_probe_status_ = false;

  if (retry_policy_.max_retries > successive_retry_count_) {
    base::TimeDelta interval = ComputeNextTimeDeltaForBackoff(
        retry_policy_.backoff, retry_policy_.base_interval,
        successive_retry_count_);

    retry_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
    // base::Unretained is safe because |retry_timer_| is owned by this.
    retry_timer_->Start(FROM_HERE, interval,
                        base::BindOnce(&PreviewsProber::CreateAndStartURLLoader,
                                       base::Unretained(this)));

    successive_retry_count_++;
    return;
  }

  is_active_ = false;
}

void PreviewsProber::ProcessProbeSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(is_active_);

  is_active_ = false;
  last_probe_status_ = true;
  successive_retry_count_ = 0;
}

base::Optional<bool> PreviewsProber::LastProbeWasSuccessful() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_probe_status_;
}
