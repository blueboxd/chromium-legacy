// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include <math.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

std::string NameForClient(PreviewsProber::ClientName name) {
  switch (name) {
    case PreviewsProber::ClientName::kLitepages:
      return "litepages";
  }
}

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

#if defined(OS_ANDROID)
bool IsInForeground(base::android::ApplicationState state) {
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return true;
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return false;
  }
}
#endif

}  // namespace

PreviewsProber::RetryPolicy::RetryPolicy() = default;
PreviewsProber::RetryPolicy::~RetryPolicy() = default;
PreviewsProber::RetryPolicy::RetryPolicy(PreviewsProber::RetryPolicy const&) =
    default;
PreviewsProber::TimeoutPolicy::TimeoutPolicy() = default;
PreviewsProber::TimeoutPolicy::~TimeoutPolicy() = default;
PreviewsProber::TimeoutPolicy::TimeoutPolicy(
    PreviewsProber::TimeoutPolicy const&) = default;

PreviewsProber::PreviewsProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy)
    : PreviewsProber(delegate,
                     url_loader_factory,
                     name,
                     url,
                     http_method,
                     headers,
                     retry_policy,
                     timeout_policy,
                     base::DefaultTickClock::GetInstance()) {}

PreviewsProber::PreviewsProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy,
    const base::TickClock* tick_clock)
    : delegate_(delegate),
      name_(NameForClient(name)),
      url_(url),
      http_method_(http_method),
      headers_(headers),
      retry_policy_(retry_policy),
      timeout_policy_(timeout_policy),
      successive_retry_count_(0),
      successive_timeout_count_(0),
      tick_clock_(tick_clock),
      is_active_(false),
      last_probe_status_(base::nullopt),
      network_connection_tracker_(nullptr),
      url_loader_factory_(url_loader_factory),
      weak_factory_(this) {
  DCHECK(delegate_);

  // The NetworkConnectionTracker can only be used directly on the UI thread.
  // Otherwise we use the cross-thread call.
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) &&
      content::GetNetworkConnectionTracker()) {
    AddSelfAsNetworkConnectionObserver(content::GetNetworkConnectionTracker());
  } else {
    content::GetNetworkConnectionTrackerFromUIThread(
        base::BindOnce(&PreviewsProber::AddSelfAsNetworkConnectionObserver,
                       weak_factory_.GetWeakPtr()));
  }
}

PreviewsProber::~PreviewsProber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void PreviewsProber::AddSelfAsNetworkConnectionObserver(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void PreviewsProber::ResetState() {
  is_active_ = false;
  successive_retry_count_ = 0;
  successive_timeout_count_ = 0;
  retry_timer_.reset();
  timeout_timer_.reset();
  url_loader_.reset();
#if defined(OS_ANDROID)
  application_status_listener_.reset();
#endif
}

void PreviewsProber::SendNowIfInactive(bool send_only_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_active_)
    return;

#if defined(OS_ANDROID)
  if (send_only_in_foreground &&
      !IsInForeground(base::android::ApplicationStatusListener::GetState())) {
    // base::Unretained is safe here because the callback is owned by
    // |application_status_listener_| which is owned by |this|.
    application_status_listener_ =
        base::android::ApplicationStatusListener::New(base::BindRepeating(
            &PreviewsProber::OnApplicationStateChange, base::Unretained(this)));
    return;
  }
#endif

  CreateAndStartURLLoader();
}

#if defined(OS_ANDROID)
void PreviewsProber::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(application_status_listener_);

  if (!IsInForeground(new_state))
    return;

  SendNowIfInactive(false);
  application_status_listener_.reset();
}
#endif

void PreviewsProber::OnConnectionChanged(network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a probe is already in flight we don't want to continue to use it since
  // the network has just changed. Reset all state and start again.
  ResetState();
  CreateAndStartURLLoader();
}

void PreviewsProber::CreateAndStartURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_active_ || successive_retry_count_ > 0);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!url_loader_);

  if (!delegate_->ShouldSendNextProbe()) {
    ResetState();
    return;
  }

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
  request->headers = headers_;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->allow_credentials = false;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PreviewsProber::OnURLLoadComplete,
                     base::Unretained(this)),
      1024);

  // We don't use SimpleURLLoader's timeout functionality because it is not
  // possible to test by PreviewsProberTest.
  base::TimeDelta ttl = ComputeNextTimeDeltaForBackoff(
      timeout_policy_.backoff, timeout_policy_.base_timeout,
      successive_timeout_count_);
  timeout_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  // base::Unretained is safe because |timeout_timer_| is owned by this.
  timeout_timer_->Start(FROM_HERE, ttl,
                        base::BindOnce(&PreviewsProber::ProcessProbeTimeout,
                                       base::Unretained(this)));
}

void PreviewsProber::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  bool was_successful = delegate_->IsResponseSuccess(
      static_cast<net::Error>(url_loader_->NetError()),
      *url_loader_->ResponseInfo(), std::move(response_body));

  timeout_timer_.reset();
  url_loader_.reset();
  successive_timeout_count_ = 0;

  if (was_successful) {
    ProcessProbeSuccess();
    return;
  }
  ProcessProbeFailure();
}

void PreviewsProber::ProcessProbeTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_);

  url_loader_.reset();
  successive_timeout_count_++;
  ProcessProbeFailure();
}

void PreviewsProber::ProcessProbeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
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

  ResetState();
}

void PreviewsProber::ProcessProbeSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
  DCHECK(is_active_);

  last_probe_status_ = true;
  ResetState();
}

base::Optional<bool> PreviewsProber::LastProbeWasSuccessful() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_probe_status_;
}
