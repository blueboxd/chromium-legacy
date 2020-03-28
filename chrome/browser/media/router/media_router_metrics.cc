// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include <algorithm>

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace media_router {

namespace {

PresentationUrlType GetPresentationUrlType(const GURL& url) {
  if (url.SchemeIs(kDialPresentationUrlScheme))
    return PresentationUrlType::kDial;
  if (url.SchemeIs(kCastPresentationUrlScheme))
    return PresentationUrlType::kCast;
  if (url.SchemeIs(kCastDialPresentationUrlScheme))
    return PresentationUrlType::kCastDial;
  if (url.SchemeIs(kRemotePlaybackPresentationUrlScheme))
    return PresentationUrlType::kRemotePlayback;
  if (base::StartsWith(url.spec(), kLegacyCastPresentationUrlPrefix,
                       base::CompareCase::INSENSITIVE_ASCII))
    return PresentationUrlType::kCastLegacy;
  if (url.SchemeIs(url::kHttpsScheme))
    return PresentationUrlType::kHttps;
  if (url.SchemeIs(url::kHttpScheme))
    return PresentationUrlType::kHttp;
  return PresentationUrlType::kOther;
}

DialogActivationLocationAndCastMode GetActivationLocationAndCastMode(
    MediaRouterDialogOpenOrigin activation_location,
    MediaCastMode cast_mode,
    bool is_icon_pinned) {
  switch (activation_location) {
    case MediaRouterDialogOpenOrigin::TOOLBAR:
      if (is_icon_pinned) {
        switch (cast_mode) {
          case MediaCastMode::PRESENTATION:
            return DialogActivationLocationAndCastMode::
                kPinnedIconAndPresentation;
          case MediaCastMode::TAB_MIRROR:
            return DialogActivationLocationAndCastMode::kPinnedIconAndTabMirror;
          case MediaCastMode::DESKTOP_MIRROR:
            return DialogActivationLocationAndCastMode::
                kPinnedIconAndDesktopMirror;
          case MediaCastMode::LOCAL_FILE:
            return DialogActivationLocationAndCastMode::kPinnedIconAndLocalFile;
        }
      } else {
        switch (cast_mode) {
          case MediaCastMode::PRESENTATION:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndPresentation;
          case MediaCastMode::TAB_MIRROR:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndTabMirror;
          case MediaCastMode::DESKTOP_MIRROR:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndDesktopMirror;
          case MediaCastMode::LOCAL_FILE:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndLocalFile;
        }
      }
      break;
    case MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kContextMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kContextMenuAndLocalFile;
      }
      break;
    case MediaRouterDialogOpenOrigin::PAGE:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kPageAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kPageAndLocalFile;
      }
      break;
    case MediaRouterDialogOpenOrigin::APP_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kAppMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kAppMenuAndLocalFile;
      }
      break;
    // |OVERFLOW_MENU| refers to extension icons hidden in the app menu. That
    // mode is no longer available for the Cast toolbar icon.
    case MediaRouterDialogOpenOrigin::OVERFLOW_MENU:
    case MediaRouterDialogOpenOrigin::TOTAL_COUNT:
      break;
  }
  NOTREACHED();
  return DialogActivationLocationAndCastMode::kMaxValue;
}

}  // namespace

MediaRouterMetrics::MediaRouterMetrics() {}
MediaRouterMetrics::~MediaRouterMetrics() = default;

// static
const char MediaRouterMetrics::kHistogramCloseLatency[] =
    "MediaRouter.Ui.Action.CloseLatency";
const char MediaRouterMetrics::kHistogramCloudPrefAtDialogOpen[] =
    "MediaRouter.Cloud.PrefAtDialogOpen";
const char MediaRouterMetrics::kHistogramCloudPrefAtInit[] =
    "MediaRouter.Cloud.PrefAtInit";
const char MediaRouterMetrics::kHistogramDialParsingError[] =
    "MediaRouter.Dial.ParsingError";
const char MediaRouterMetrics::kHistogramDialFetchAppInfo[] =
    "MediaRouter.Dial.FetchAppInfo";
const char MediaRouterMetrics::kHistogramIconClickLocation[] =
    "MediaRouter.Icon.Click.Location";
const char MediaRouterMetrics::kHistogramMediaRouterCastingSource[] =
    "MediaRouter.Source.CastingSource";
const char MediaRouterMetrics::kHistogramMediaRouterFileFormat[] =
    "MediaRouter.Source.LocalFileFormat";
const char MediaRouterMetrics::kHistogramMediaRouterFileSize[] =
    "MediaRouter.Source.LocalFileSize";
const char MediaRouterMetrics::kHistogramMediaSinkType[] =
    "MediaRouter.Sink.SelectedType";
const char MediaRouterMetrics::kHistogramPresentationUrlType[] =
    "MediaRouter.PresentationRequest.AvailabilityUrlType";
const char MediaRouterMetrics::kHistogramRouteCreationOutcome[] =
    "MediaRouter.Route.CreationOutcome";
const char MediaRouterMetrics::kHistogramStartLocalLatency[] =
    "MediaRouter.Ui.Action.StartLocal.Latency";
const char MediaRouterMetrics::kHistogramStartLocalPosition[] =
    "MediaRouter.Ui.Action.StartLocalPosition";
const char MediaRouterMetrics::kHistogramStartLocalSessionSuccessful[] =
    "MediaRouter.Ui.Action.StartLocalSessionSuccessful";
const char MediaRouterMetrics::kHistogramStopRoute[] =
    "MediaRouter.Ui.Action.StopRoute";
const char MediaRouterMetrics::kHistogramUiDeviceCount[] =
    "MediaRouter.Ui.Device.Count";
const char
    MediaRouterMetrics::kHistogramUiDialogActivationLocationAndCastMode[] =
        "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode";
const char MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen[] =
    "MediaRouter.Ui.Dialog.IconStateAtOpen";
const char MediaRouterMetrics::kHistogramUiDialogLoadedWithData[] =
    "MediaRouter.Ui.Dialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiDialogPaint[] =
    "MediaRouter.Ui.Dialog.Paint";
const char MediaRouterMetrics::kHistogramUiFirstAction[] =
    "MediaRouter.Ui.FirstAction";
const char MediaRouterMetrics::kHistogramUiIconStateAtInit[] =
    "MediaRouter.Ui.IconStateAtInit";

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  DCHECK_LT(static_cast<int>(origin),
            static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramIconClickLocation, static_cast<int>(origin),
      static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogPaint(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogPaint, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogLoaded(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogLoadedWithData, delta);
}

// static
void MediaRouterMetrics::RecordCloseDialogLatency(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramCloseLatency, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterInitialUserAction(
    MediaRouterUserAction action) {
  DCHECK_LT(static_cast<int>(action),
            static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramUiFirstAction, static_cast<int>(action),
      static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordRouteCreationOutcome(
    MediaRouterRouteCreationOutcome outcome) {
  DCHECK_LT(static_cast<int>(outcome),
            static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramRouteCreationOutcome, static_cast<int>(outcome),
      static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterCastingSource(MediaCastMode source) {
  base::UmaHistogramSparse(kHistogramMediaRouterCastingSource, source);
}

// static
void MediaRouterMetrics::RecordMediaRouterFileFormat(
    const media::container_names::MediaContainerName format) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMediaRouterFileFormat, format,
                            media::container_names::CONTAINER_MAX + 1);
}

// static
void MediaRouterMetrics::RecordMediaRouterFileSize(int64_t size) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB(kHistogramMediaRouterFileSize, size);
}

// static
void MediaRouterMetrics::RecordDialParsingError(
    SafeDialDeviceDescriptionParser::ParsingError parsing_error) {
  DCHECK_LT(parsing_error,
            SafeDialDeviceDescriptionParser::ParsingError::kTotalCount);
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramDialParsingError, parsing_error,
      SafeDialDeviceDescriptionParser::ParsingError::kTotalCount);
}

// static
void MediaRouterMetrics::RecordDialFetchAppInfo(
    DialAppInfoResultCode result_code) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramDialFetchAppInfo, result_code,
                            DialAppInfoResultCode::kCount);
}

// static
void MediaRouterMetrics::RecordPresentationUrlType(const GURL& url) {
  PresentationUrlType type = GetPresentationUrlType(url);
  UMA_HISTOGRAM_ENUMERATION(kHistogramPresentationUrlType, type,
                            PresentationUrlType::kPresentationUrlTypeCount);
}

// static
void MediaRouterMetrics::RecordMediaSinkType(SinkIconType sink_icon_type) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMediaSinkType, sink_icon_type,
                            SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordDeviceCount(int device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramUiDeviceCount, device_count);
}

// static
void MediaRouterMetrics::RecordStartRouteDeviceIndex(int index) {
  base::UmaHistogramSparse(kHistogramStartLocalPosition, std::min(index, 100));
}

// static
void MediaRouterMetrics::RecordStartLocalSessionLatency(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramStartLocalLatency, delta);
}

// static
void MediaRouterMetrics::RecordStartLocalSessionSuccessful(bool success) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramStartLocalSessionSuccessful, success);
}

// static
void MediaRouterMetrics::RecordStopLocalRoute() {
  // Local routes have the enum value 0.
  UMA_HISTOGRAM_BOOLEAN(kHistogramStopRoute, 0);
}

// static
void MediaRouterMetrics::RecordStopRemoteRoute() {
  // Remote routes have the enum value 1.
  UMA_HISTOGRAM_BOOLEAN(kHistogramStopRoute, 1);
}

// static
void MediaRouterMetrics::RecordIconStateAtDialogOpen(bool is_pinned) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramUiDialogIconStateAtOpen, is_pinned);
}

// static
void MediaRouterMetrics::RecordIconStateAtInit(bool is_pinned) {
  // Since this gets called only rarely, use base::UmaHistogramBoolean() to
  // avoid instantiating the caching code.
  base::UmaHistogramBoolean(kHistogramUiIconStateAtInit, is_pinned);
}

// static
void MediaRouterMetrics::RecordCloudPrefAtDialogOpen(bool enabled) {
  base::UmaHistogramBoolean(kHistogramCloudPrefAtDialogOpen, enabled);
}

// static
void MediaRouterMetrics::RecordCloudPrefAtInit(bool enabled) {
  base::UmaHistogramBoolean(kHistogramCloudPrefAtInit, enabled);
}

// static
void MediaRouterMetrics::RecordDialogActivationLocationAndCastMode(
    MediaRouterDialogOpenOrigin activation_location,
    MediaCastMode cast_mode,
    bool is_icon_pinned) {
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramUiDialogActivationLocationAndCastMode,
      GetActivationLocationAndCastMode(activation_location, cast_mode,
                                       is_icon_pinned));
}

}  // namespace media_router
