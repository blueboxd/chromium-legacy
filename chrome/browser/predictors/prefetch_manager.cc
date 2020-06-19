// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/prefetch_manager.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/empty_url_loader_client.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace predictors {

namespace {

// TODO(crbug.com/1095842): Update the annotation once URL allowlist/blocklist
// are observed to limit the scope of the requests.
const net::NetworkTrafficAnnotationTag kPrefetchTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("predictive_prefetch",
                                        R"(
    semantics {
      sender: "Loading Predictor"
      description:
        "This request is issued near the start of a navigation to "
        "speculatively fetch resources that resulting page is predicted to "
        "request."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies and site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Advanced > "
        "   Privacy > Preload pages for faster browsing and searching,"
        "B) Disable Lite Mode under Settings > Advanced > Lite mode, or "
        "C) Disable 'Make searches and browsing better' under Settings > "
        "   Sync and Google services > Make searches and browsing better"
      policy_exception_justification: "To be implemented"
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster page loads."
)");

}  // namespace

PrefetchStats::PrefetchStats(const GURL& url)
    : url(url), start_time(base::TimeTicks::Now()) {}
PrefetchStats::~PrefetchStats() = default;

PrefetchInfo::PrefetchInfo(const GURL& url, size_t count)
    : url(url),
      queued_count(count),
      stats(std::make_unique<PrefetchStats>(url)) {
  DCHECK(url.is_valid());
  DCHECK(url.SchemeIsHTTPOrHTTPS());
}

PrefetchInfo::~PrefetchInfo() = default;

PrefetchJob::PrefetchJob(PrefetchRequest prefetch_request, PrefetchInfo* info)
    : url(prefetch_request.url),
      network_isolation_key(std::move(prefetch_request.network_isolation_key)),
      info(info) {
  DCHECK(url.is_valid());
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  DCHECK(network_isolation_key.IsFullyPopulated());
}

PrefetchJob::~PrefetchJob() = default;

PrefetchManager::PrefetchManager(base::WeakPtr<Delegate> delegate,
                                 Profile* profile)
    : delegate_(std::move(delegate)), profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
}

PrefetchManager::~PrefetchManager() = default;

void PrefetchManager::Start(const GURL& url,
                            std::vector<PrefetchRequest> requests) {
  DCHECK(base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch));
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefetchInfo* info;
  if (prefetch_info_.find(url) == prefetch_info_.end()) {
    auto iterator_and_whether_inserted = prefetch_info_.emplace(
        url, std::make_unique<PrefetchInfo>(url, requests.size()));
    info = iterator_and_whether_inserted.first->second.get();
  } else {
    info = prefetch_info_.find(url)->second.get();
    info->queued_count += requests.size();
  }

  for (auto& request : requests) {
    PrefetchJobId job_id =
        jobs_.Add(std::make_unique<PrefetchJob>(std::move(request), info));
    queued_jobs_.push_back(job_id);
  }

  TryToLaunchPrefetchJobs();
}

size_t PrefetchManager::inflight_jobs_count() const {
  DCHECK_GE(jobs_.size(), queued_jobs_.size());
  return jobs_.size() - queued_jobs_.size();
}

void PrefetchManager::PrefetchUrl(PrefetchInfo& info,
                                  const GURL& prefetch_url,
                                  PrefetchJobId job_id,
                                  network::SharedURLLoaderFactory& factory) {
  url::Origin top_frame_origin = url::Origin::Create(info.url);

  network::ResourceRequest request;
  request.method = "GET";
  request.url = prefetch_url;
  request.site_for_cookies = net::SiteForCookies::FromUrl(info.url);
  request.request_initiator = top_frame_origin;
  request.referrer = info.url;

  request.headers.SetHeader("Purpose", "prefetch");

  request.load_flags = net::LOAD_PREFETCH;
  // TODO(falken): Get the real resource type from the hint and set
  // |destination| too.
  // https://source.chromium.org/chromium/chromium/src/+/master:components/optimization_guide/proto/loading_predictor_metadata.proto;l=13;drc=f59ec65870df7152bcffa34e2b3f1923de07fea8
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);

  // TODO(falken): Support CORS?
  request.mode = network::mojom::RequestMode::kNoCors;

  // The hints are only for requests made from the top frame,
  // so frame_origin is the same as top_frame_origin.
  auto frame_origin = top_frame_origin;

  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateNothing, top_frame_origin,
      frame_origin, net::SiteForCookies::FromUrl(info.url));

  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client;
  auto client_receiver = url_loader_client.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::URLLoader> loader;

  // TODO(crbug.com/1095842): Use throttles. Call
  // content::CreateContentBrowserURLLoaderThrottles() to get necessary
  // throttles and use blink::ThrottlingURLLoader::CreateLoaderAndStart().

  // TODO(crbug.com/1092329): Ensure the request is seen by extensions.

  factory.CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*routing_id is not needed*/ -1,
      content::GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, request, std::move(url_loader_client),
      net::MutableNetworkTrafficAnnotationTag(kPrefetchTrafficAnnotation));

  ++info.inflight_count;

  // The idea of prefetching is for the network service to put the response in
  // the http cache. So from the prefetching layer, nothing needs to be done
  // with the response, so just drain it.
  network::EmptyURLLoaderClient::DrainURLRequest(
      std::move(client_receiver), std::move(loader),
      base::BindOnce(&PrefetchManager::OnPrefetchFinished,
                     weak_factory_.GetWeakPtr(), job_id));
}

PrefetchInfo* PrefetchManager::GetJobInfo(PrefetchJobId job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefetchJob* job = jobs_.Lookup(job_id);
  DCHECK(job);
  return job->info;
}

void PrefetchManager::OnPrefetchFinished(PrefetchJobId job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Get the info before destroying the job.
  PrefetchInfo* info = GetJobInfo(job_id);
  jobs_.Remove(job_id);

  // Clean up the job and start more if needed.
  DCHECK_GT(info->inflight_count, 0u);
  --info->inflight_count;
  if (info->is_done())
    AllPrefetchJobsForUrlFinished(info);
  TryToLaunchPrefetchJobs();
}

void PrefetchManager::TryToLaunchPrefetchJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(falken): Is it ok to assume the default partition? Try to plumb the
  // partition here, e.g., from WebContentsObserver. And make a similar change
  // in PreconnectManager.
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile_);
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();

  while (!queued_jobs_.empty() && inflight_jobs_count() < kMaxInflightJobs) {
    auto job_id = queued_jobs_.front();
    queued_jobs_.pop_front();
    PrefetchJob* job = jobs_.Lookup(job_id);
    DCHECK(job);
    PrefetchInfo* info = job->info;

    if (job->url.is_valid() && factory)
      PrefetchUrl(*info, job->url, job_id, *factory);

    DCHECK_LE(1u, info->queued_count);
    --info->queued_count;
    if (info->is_done())
      AllPrefetchJobsForUrlFinished(info);
  }
}

void PrefetchManager::AllPrefetchJobsForUrlFinished(PrefetchInfo* info) {
  DCHECK(info);
  DCHECK(info->is_done());
  auto it = prefetch_info_.find(info->url);
  DCHECK(it != prefetch_info_.end());
  DCHECK(info == it->second.get());

  if (delegate_)
    delegate_->PrefetchFinished(std::move(info->stats));
  prefetch_info_.erase(it);
}

}  // namespace predictors
