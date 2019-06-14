// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/favicon/core/favicon_server_fetcher_params.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/features.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace favicon {

namespace {

void RecordFaviconAvailabilityMetric(FaviconRequestOrigin origin,
                                     FaviconAvailability availability) {
  switch (origin) {
    case FaviconRequestOrigin::HISTORY:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.HISTORY",
                                availability);
      break;
    case FaviconRequestOrigin::HISTORY_SYNCED_TABS:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.SYNCED_TABS",
                                availability);
      break;
    case FaviconRequestOrigin::RECENTLY_CLOSED_TABS:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.RECENTLY_CLOSED_TABS",
                                availability);
      break;
    case FaviconRequestOrigin::UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.UNKNOWN",
                                availability);
      break;
  }
}

void RecordFaviconServerGroupingMetric(FaviconRequestOrigin origin,
                                       int group_size) {
  DCHECK_GE(group_size, 0);
  switch (origin) {
    case FaviconRequestOrigin::HISTORY:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.HISTORY", group_size);
      break;
    case FaviconRequestOrigin::HISTORY_SYNCED_TABS:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.SYNCED_TABS", group_size);
      break;
    case FaviconRequestOrigin::RECENTLY_CLOSED_TABS:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.RECENTLY_CLOSED_TABS",
          group_size);
      break;
    case FaviconRequestOrigin::UNKNOWN:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.UNKNOWN", group_size);
      break;
  }
}

// Parameter used for local bitmap queries by page url. The url is an origin,
// and it may not have had a favicon associated with it. A trickier case is when
// it only has domain-scoped cookies, but visitors are redirected to HTTPS on
// visiting. It defaults to a HTTP scheme, but the favicon will be associated
// with the HTTPS URL and hence won't be found if we include the scheme in the
// lookup. Set |fallback_to_host|=true so the favicon database will fall back to
// matching only the hostname to have the best chance of finding a favicon.
// TODO(victorvianna): Consider passing this as a parameter in the API.
const bool kFallbackToHost = true;

// Parameter used for local bitmap queries by page url.
favicon_base::IconTypeSet GetIconTypesForLocalQuery() {
  return {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
          favicon_base::IconType::kTouchPrecomposedIcon,
          favicon_base::IconType::kWebManifestIcon};
}

bool CanOriginQueryGoogleServer(FaviconRequestOrigin origin) {
  switch (origin) {
    case FaviconRequestOrigin::HISTORY:
    case FaviconRequestOrigin::HISTORY_SYNCED_TABS:
    case FaviconRequestOrigin::RECENTLY_CLOSED_TABS:
      return true;
    case FaviconRequestOrigin::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

GURL GetGroupIdentifier(const GURL& page_url, const GURL& icon_url) {
  // If it is not possible to find a mapped icon url, identify the group of
  // |page_url| by itself.
  return !icon_url.is_empty() ? icon_url : page_url;
}

bool CanQueryGoogleServer(FaviconRequestOrigin origin,
                          bool can_send_history_data) {
  return CanOriginQueryGoogleServer(origin) && can_send_history_data &&
         base::FeatureList::IsEnabled(kEnableHistoryFaviconsGoogleServerQuery);
}

}  // namespace

FaviconRequestHandler::FaviconRequestHandler(
    FaviconService* favicon_service,
    LargeIconService* large_icon_service)
    : favicon_service_(favicon_service),
      large_icon_service_(large_icon_service) {
  DCHECK(favicon_service);
  DCHECK(large_icon_service);
}

FaviconRequestHandler::~FaviconRequestHandler() {}

void FaviconRequestHandler::GetRawFaviconForPageURL(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback,
    FaviconRequestOrigin request_origin,
    FaviconRequestPlatform request_platform,
    const GURL& icon_url_for_uma,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    bool can_send_history_data,
    base::CancelableTaskTracker* tracker) {
  // First attempt to find the icon locally.
  favicon_service_->GetRawFaviconForPageURL(
      page_url, GetIconTypesForLocalQuery(), desired_size_in_pixel,
      kFallbackToHost,
      base::BindOnce(
          &FaviconRequestHandler::OnBitmapLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url, desired_size_in_pixel,
          /*response_callback=*/std::move(callback), request_origin,
          request_platform, icon_url_for_uma, std::move(synced_favicon_getter),
          CanQueryGoogleServer(request_origin, can_send_history_data), tracker),
      tracker);
}

void FaviconRequestHandler::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    FaviconRequestOrigin request_origin,
    const GURL& icon_url_for_uma,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    bool can_send_history_data,
    base::CancelableTaskTracker* tracker) {
  // First attempt to find the icon locally.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindOnce(
          &FaviconRequestHandler::OnImageLocalDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), page_url,
          /*response_callback=*/std::move(callback), request_origin,
          icon_url_for_uma, std::move(synced_favicon_getter),
          CanQueryGoogleServer(request_origin, can_send_history_data), tracker),
      tracker);
}

void FaviconRequestHandler::OnBitmapLocalDataAvailable(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback response_callback,
    FaviconRequestOrigin origin,
    FaviconRequestPlatform platform,
    const GURL& icon_url_for_uma,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    bool can_query_google_server,
    base::CancelableTaskTracker* tracker,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(bitmap_result.icon_url);
    std::move(response_callback).Run(bitmap_result);
    return;
  }

  if (can_query_google_server) {
    // TODO(victorvianna): Avoid using AdaptCallbackForRepeating.
    base::RepeatingCallback<void(const favicon_base::FaviconRawBitmapResult&)>
        repeating_response_callback =
            base::AdaptCallbackForRepeating(std::move(response_callback));
    // TODO(victorvianna): Set |min_source_size_in_pixel| correctly.
    std::unique_ptr<FaviconServerFetcherParams> server_parameters =
        platform == FaviconRequestPlatform::kMobile
            ? FaviconServerFetcherParams::CreateForMobile(
                  page_url, /*min_source_size_in_pixel=*/1,
                  desired_size_in_pixel)
            : FaviconServerFetcherParams::CreateForDesktop(page_url);
    RequestFromGoogleServer(
        page_url, std::move(server_parameters),
        /*empty_response_callback=*/
        base::BindOnce(repeating_response_callback,
                       favicon_base::FaviconRawBitmapResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetRawFaviconForPageURL),
            base::Unretained(favicon_service_), page_url,
            GetIconTypesForLocalQuery(), desired_size_in_pixel, kFallbackToHost,
            repeating_response_callback, tracker),
        icon_url_for_uma, origin);
    return;
  }

  scoped_refptr<base::RefCountedMemory> sync_bitmap =
      std::move(synced_favicon_getter).Run(page_url);
  if (sync_bitmap) {
    // If request to sync succeeds, send the retrieved bitmap.
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kSync);
    favicon_base::FaviconRawBitmapResult sync_bitmap_result;
    sync_bitmap_result.bitmap_data = sync_bitmap;
    std::move(response_callback).Run(sync_bitmap_result);
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconRawBitmapResult());
}

void FaviconRequestHandler::OnImageLocalDataAvailable(
    const GURL& page_url,
    favicon_base::FaviconImageCallback response_callback,
    FaviconRequestOrigin origin,
    const GURL& icon_url_for_uma,
    FaviconRequestHandler::SyncedFaviconGetter synced_favicon_getter,
    bool can_query_google_server,
    base::CancelableTaskTracker* tracker,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    // The icon comes from local storage now even though it may have been
    // originally retrieved from Google server.
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kLocal);
    large_icon_service_->TouchIconFromGoogleServer(image_result.icon_url);
    std::move(response_callback).Run(image_result);
    return;
  }

  if (can_query_google_server) {
    // TODO(victorvianna): Avoid using AdaptCallbackForRepeating.
    base::RepeatingCallback<void(const favicon_base::FaviconImageResult&)>
        repeating_response_callback =
            base::AdaptCallbackForRepeating(std::move(response_callback));
    // We use CreateForDesktop because GetFaviconImageForPageURL is only called
    // by desktop.
    RequestFromGoogleServer(
        page_url, FaviconServerFetcherParams::CreateForDesktop(page_url),
        /*empty_response_callback=*/
        base::BindOnce(repeating_response_callback,
                       favicon_base::FaviconImageResult()),
        /*local_lookup_callback=*/
        base::BindOnce(
            base::IgnoreResult(&FaviconService::GetFaviconImageForPageURL),
            base::Unretained(favicon_service_), page_url,
            repeating_response_callback, tracker),
        icon_url_for_uma, origin);
    return;
  }

  scoped_refptr<base::RefCountedMemory> sync_bitmap =
      std::move(synced_favicon_getter).Run(page_url);
  if (sync_bitmap) {
    // If request to sync succeeds, convert the retrieved bitmap to image and
    // send.
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kSync);
    favicon_base::FaviconImageResult sync_image_result;
    sync_image_result.image =
        gfx::Image::CreateFrom1xPNGBytes(sync_bitmap.get());
    std::move(response_callback).Run(sync_image_result);
    return;
  }

  // If sync does not have the favicon, send empty response.
  RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kNotAvailable);
  std::move(response_callback).Run(favicon_base::FaviconImageResult());
}

void FaviconRequestHandler::RequestFromGoogleServer(
    const GURL& page_url,
    std::unique_ptr<FaviconServerFetcherParams> server_parameters,
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    const GURL& icon_url_for_uma,
    FaviconRequestOrigin origin) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("favicon_request_handler_get_favicon",
                                          R"(
      semantics {
        sender: "Favicon Request Handler"
        description:
          "Sends a request to Google favicon server to rerieve the favicon "
          "bitmap for an entry in the history UI or the recent tabs menu UI."
        trigger:
          "The user visits chrome://history, chrome://history/syncedTabs or "
          "uses the Recent Tabs menu. A request can be sent if Chrome does not "
          "have a favicon for a particular page url. Only happens if history "
          "sync (with no custom passphrase) is enabled."
        data: "Page URL and desired icon size."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can disable this by toggling Sync and Google services > Manage "
          "sync > History"
        chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
      })");
  // Increase count of theoretical waiting callbacks in the group of |page_url|.
  GURL group_identifier = GetGroupIdentifier(page_url, icon_url_for_uma);
  int group_count =
      ++group_callbacks_count_[group_identifier];  // Map defaults to 0.
  // If grouping was implemented, only the first requested page url of a group
  // would be effectively sent to the server and become responsible for calling
  // all callbacks in its group once done.
  GURL group_to_clear = group_count == 1 ? group_identifier : GURL();
  // If |trim_url_path| parameter in the experiment is true, the path part of
  // the url is stripped in the request, but result is stored under the complete
  // url.
  bool should_trim_url_path = base::GetFieldTrialParamByFeatureAsBool(
      kEnableHistoryFaviconsGoogleServerQuery, "trim_url_path",
      /* default_value= */ false);
  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          std::move(server_parameters),
          /*may_page_url_be_private=*/true, should_trim_url_path,
          traffic_annotation,
          base::BindOnce(&FaviconRequestHandler::OnGoogleServerDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(empty_response_callback),
                         std::move(local_lookup_callback), origin,
                         group_to_clear));
}

void FaviconRequestHandler::OnGoogleServerDataAvailable(
    base::OnceClosure empty_response_callback,
    base::OnceClosure local_lookup_callback,
    FaviconRequestOrigin origin,
    const GURL& group_to_clear,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  if (!group_to_clear.is_empty()) {
    auto it = group_callbacks_count_.find(group_to_clear);
    RecordFaviconServerGroupingMetric(origin, it->second);
    group_callbacks_count_.erase(it);
  }
  // When parallel requests return the same icon url, we get FAILURE_ON_WRITE,
  // since we're trying to write repeated values to the DB. Or if a write
  // operation to that icon url was already completed, we may get status
  // FAILURE_ICON_EXISTS_IN_DB. In both cases we're able to subsequently
  // retrieve the icon from local storage.
  if (status == favicon_base::GoogleFaviconServerRequestStatus::SUCCESS ||
      status ==
          favicon_base::GoogleFaviconServerRequestStatus::FAILURE_ON_WRITE ||
      status == favicon_base::GoogleFaviconServerRequestStatus::
                    FAILURE_ICON_EXISTS_IN_DB) {
    // We record the metrics as kLocal since the icon will be subsequently
    // retrieved from local storage.
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kLocal);
    std::move(local_lookup_callback).Run();
  } else {
    RecordFaviconAvailabilityMetric(origin, FaviconAvailability::kNotAvailable);
    std::move(empty_response_callback).Run();
  }
}

}  // namespace favicon
