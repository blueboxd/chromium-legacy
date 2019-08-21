// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"

#include "base/base64.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

void FlushMetricsForNavigation(
    OptimizationGuideNavigationData* navigation_data) {
  if (!navigation_data->serialized_hint_version_string().has_value() ||
      navigation_data->serialized_hint_version_string().value().empty())
    return;

  // Deserialize the serialized version string into its protobuffer.
  std::string binary_version_pb;
  if (!base::Base64Decode(
          navigation_data->serialized_hint_version_string().value(),
          &binary_version_pb))
    return;

  optimization_guide::proto::Version hint_version;
  if (!hint_version.ParseFromString(binary_version_pb))
    return;

  // Record the UKM.
  ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
      navigation_data->navigation_id(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::OptimizationGuide builder(ukm_source_id);
  if (hint_version.has_generation_timestamp() &&
      hint_version.generation_timestamp().seconds() > 0) {
    builder.SetHintGenerationTimestamp(
        hint_version.generation_timestamp().seconds());
  }
  if (hint_version.has_hint_source() &&
      hint_version.hint_source() !=
          optimization_guide::proto::HINT_SOURCE_UNKNOWN) {
    builder.SetHintSource(static_cast<int>(hint_version.hint_source()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace

OptimizationGuideWebContentsObserver::OptimizationGuideWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

OptimizationGuideWebContentsObserver::~OptimizationGuideWebContentsObserver() =
    default;

OptimizationGuideNavigationData* OptimizationGuideWebContentsObserver::
    GetOrCreateOptimizationGuideNavigationData(
        content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int64_t navigation_id = navigation_handle->GetNavigationId();
  if (inflight_optimization_guide_navigation_datas_.find(navigation_id) ==
      inflight_optimization_guide_navigation_datas_.end()) {
    // We do not have one already - create one.
    inflight_optimization_guide_navigation_datas_.emplace(
        std::piecewise_construct, std::forward_as_tuple(navigation_id),
        std::forward_as_tuple(navigation_id));
  }

  DCHECK(inflight_optimization_guide_navigation_datas_.find(navigation_id) !=
         inflight_optimization_guide_navigation_datas_.end());
  return &(inflight_optimization_guide_navigation_datas_.find(navigation_id)
               ->second);
}

void OptimizationGuideWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delete Optimization Guide information later, so that other
  // DidFinishNavigation methods can reliably use
  // GetOptimizationGuideNavigationData regardless of order of
  // WebContentsObservers.
  // Note that a lot of Navigations (sub-frames, same document, non-committed,
  // etc.) might not have navigation data associated with them, but we reduce
  // likelihood of future leaks by always trying to remove the data.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OptimizationGuideWebContentsObserver::
                         FlushMetricsAndRemoveOptimizationGuideNavigationData,
                     weak_factory_.GetWeakPtr(),
                     navigation_handle->GetNavigationId()));
}

void OptimizationGuideWebContentsObserver::
    FlushMetricsAndRemoveOptimizationGuideNavigationData(
        int64_t navigation_id) {
  auto nav_data_iter =
      inflight_optimization_guide_navigation_datas_.find(navigation_id);
  if (nav_data_iter == inflight_optimization_guide_navigation_datas_.end())
    return;

  FlushMetricsForNavigation(&(nav_data_iter->second));

  inflight_optimization_guide_navigation_datas_.erase(navigation_id);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver)
