// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"

namespace media_router {

namespace {

constexpr char kHistogramProviderCreateRouteResult[] =
    "MediaRouter.Provider.CreateRoute.Result";
constexpr char kHistogramProviderJoinRouteResult[] =
    "MediaRouter.Provider.JoinRoute.Result";
constexpr char kHistogramProviderRouteControllerCreationOutcome[] =
    "MediaRouter.Provider.RouteControllerCreationOutcome";
constexpr char kHistogramProviderTerminateRouteResult[] =
    "MediaRouter.Provider.TerminateRoute.Result";
constexpr char kHistogramProviderVersion[] = "MediaRouter.Provider.Version";
constexpr char kHistogramProviderWakeReason[] =
    "MediaRouter.Provider.WakeReason";
constexpr char kHistogramProviderWakeup[] = "MediaRouter.Provider.Wakeup";

std::string GetHistogramNameForProvider(const std::string& base_name,
                                        MediaRouteProviderId provider_id) {
  switch (provider_id) {
    case MediaRouteProviderId::CAST:
      return base_name + ".Cast";
    case MediaRouteProviderId::DIAL:
      return base_name + ".DIAL";
    case MediaRouteProviderId::WIRED_DISPLAY:
      return base_name + ".WiredDisplay";
    // |EXTENSION| and |UNKNOWN| use the base histogram name.
    case MediaRouteProviderId::EXTENSION:
    case MediaRouteProviderId::UNKNOWN:
      return base_name;
  }
}

}  // namespace

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK_LT(static_cast<int>(reason),
            static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderWakeReason, reason,
                                MediaRouteProviderWakeReason::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(
    const extensions::Extension& extension) {
  MediaRouteProviderVersion version = MediaRouteProviderVersion::UNKNOWN;
  version = GetMediaRouteProviderVersion(extension.version(),
                                         version_info::GetVersion());

  DCHECK_LT(static_cast<int>(version),
            static_cast<int>(MediaRouteProviderVersion::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderVersion, version,
                                MediaRouteProviderVersion::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
    MediaRouteProviderWakeup wakeup) {
  DCHECK_LT(static_cast<int>(wakeup),
            static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderWakeup, wakeup,
                                MediaRouteProviderWakeup::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordCreateRouteResultCode(
    MediaRouteProviderId provider_id,
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderCreateRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordJoinRouteResultCode(
    MediaRouteProviderId provider_id,
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderJoinRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute(
    MediaRouteProviderId provider_id,
    RouteRequestResult::ResultCode result_code) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderTerminateRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteControllerCreationResult(
    bool success) {
  base::UmaHistogramBoolean(kHistogramProviderRouteControllerCreationOutcome,
                            success);
}

// static
MediaRouteProviderVersion MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
    const base::Version& extension_version,
    const base::Version& browser_version) {
  if (!extension_version.IsValid() || extension_version.components().empty() ||
      !browser_version.IsValid() || browser_version.components().empty()) {
    return MediaRouteProviderVersion::UNKNOWN;
  }

  uint32_t extension_major = extension_version.components()[0];
  uint32_t browser_major = browser_version.components()[0];
  // Sanity check.
  if (extension_major == 0 || browser_major == 0) {
    return MediaRouteProviderVersion::UNKNOWN;
  } else if (extension_major >= browser_major) {
    return MediaRouteProviderVersion::SAME_VERSION_AS_CHROME;
  } else if (browser_major - extension_major == 1) {
    return MediaRouteProviderVersion::ONE_VERSION_BEHIND_CHROME;
  } else {
    return MediaRouteProviderVersion::MULTIPLE_VERSIONS_BEHIND_CHROME;
  }
}

}  // namespace media_router
