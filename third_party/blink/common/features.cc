// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "build/build_config.h"
#include "services/network/public/cpp/features.h"

namespace blink {
namespace features {

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
const base::Feature kBlockingDownloadsInAdFrameWithoutUserActivation{
    "BlockingDownloadsInAdFrameWithoutUserActivation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable defer commits a bit to avoid flash.
const base::Feature kAvoidFlashBetweenNavigation{
    "AvoidFlashBetweenNavigation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the user-specified viewport restriction for GPU Rasterization on
// mobile. See https://crbug.com/899399
const base::Feature kEnableGpuRasterizationViewportRestriction{
    "EnableGpuRasterizationViewportRestriction",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls script streaming.
const base::Feature kScriptStreaming{"ScriptStreaming",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Allow streaming small (<30kB) scripts.
const base::Feature kSmallScriptStreaming{"SmallScriptStreaming",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables user level memory pressure signal generation on Android.
const base::Feature kUserLevelMemoryPressureSignal{
    "UserLevelMemoryPressureSignal", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable FCP++ by experiment. See https://crbug.com/869924
const base::Feature kFirstContentfulPaintPlusPlus{
    "FirstContentfulPaintPlusPlus", base::FEATURE_ENABLED_BY_DEFAULT};

// Perform memory purges after freezing only if all pages are frozen.
const base::Feature kFreezePurgeMemoryAllPagesFrozen{
    "FreezePurgeMemoryAllPagesFrozen", base::FEATURE_DISABLED_BY_DEFAULT};

// Freezes the user-agent as part of https://github.com/WICG/ua-client-hints.
const base::Feature kFreezeUserAgent{"FreezeUserAgent",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experimental sweep-line algorithm for tracking "jank" from
// layout objects changing their visual location between animation frames.
const base::Feature kJankTrackingSweepLine{"JankTrackingSweepLine",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a new compositing mode called BlinkGenPropertyTrees where Blink
// generates the compositor property trees. See: https://crbug.com/836884.
const base::Feature kBlinkGenPropertyTrees{"BlinkGenPropertyTrees",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enable a new CSS property called backdrop-filter.
const base::Feature kCSSBackdropFilter{"CSSBackdropFilter",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Display Locking JavaScript APIs.
const base::Feature kDisplayLocking{"DisplayLocking",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable applying rounded corner masks via a GL shader rather than
// a mask layer.
const base::Feature kFastBorderRadius{"FastBorderRadius",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable mojo Blob URL interface and better blob URL lifetime management.
// Can be enabled independently of NetworkService.
const base::Feature kMojoBlobURLs{"MojoBlobURLs",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded. Additionally, navigation
// predictor may preconnect/prefetch to resources/origins to make the
// future navigations faster.
const base::Feature kNavigationPredictor{"NavigationPredictor",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable off-the-main-thread dedicated worker script fetch.
// (https://crbug.com/835717)
const base::Feature kOffMainThreadDedicatedWorkerScriptFetch{
    "OffMainThreadDedicatedWorkerScriptFetch",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable off-the-main-thread service worker script fetch.
// (https://crbug.com/924043)
const base::Feature kOffMainThreadServiceWorkerScriptFetch{
    "OffMainThreadServiceWorkerScriptFetch", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable off-the-main-thread shared worker script fetch.
// (https://crbug.com/924041)
const base::Feature kOffMainThreadSharedWorkerScriptFetch{
    "OffMainThreadSharedWorkerScriptFetch", base::FEATURE_ENABLED_BY_DEFAULT};

// Onion souping for all DOMStorage. https://crbug.com/781870
const base::Feature kOnionSoupDOMStorage{"OnionSoupDOMStorage",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
const base::Feature kPlzDedicatedWorker{"PlzDedicatedWorker",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
const base::Feature kPortals{"Portals", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable limiting previews loading hints to specific resource types.
const base::Feature kPreviewsResourceLoadingHintsSpecificResourceTypes{
    "PreviewsResourceLoadingHintsSpecificResourceTypes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Perform a memory purge after a renderer is backgrounded. Formerly labelled as
// the "PurgeAndSuspend" experiment.
//
// TODO(adityakeerthi): Disabled by default on Mac and Android for historical
// reasons. Consider enabling by default if experiment results are positive.
// https://crbug.com/926186
const base::Feature kPurgeRendererMemoryWhenBackgrounded {
  "PurgeRendererMemoryWhenBackgrounded",
#if defined(OS_MACOSX) || defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enable Implicit Root Scroller. https://crbug.com/903260.
// TODO(bokan): Temporarily disabled on desktop platforms to address issues
// with non-overlay scrollbars. https://crbug.com/948059.
const base::Feature kImplicitRootScroller {
  "ImplicitRootScroller",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables usage of getDisplayMedia() that allows capture of web content, see
// https://crbug.com/865060.
const base::Feature kRTCGetDisplayMedia{"RTCGetDisplayMedia",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Changes the default RTCPeerConnection constructor behavior to use Unified
// Plan as the SDP semantics. When the feature is enabled, Unified Plan is used
// unless the default is overridden (by passing {sdpSemantics:'plan-b'} as the
// argument).
const base::Feature kRTCUnifiedPlanByDefault{"RTCUnifiedPlanByDefault",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Determines if the SDP attrbute extmap-allow-mixed should be offered by
// default or not. The default value can be overridden by passing
// {offerExtmapAllowMixed:true} as an argument to the RTCPeerConnection
// constructor.
const base::Feature kRTCOfferExtmapAllowMixed{
    "RTCOfferExtmapAllowMixed", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kServiceWorkerIsolateInForeground{
    "ServiceWorkerIsolateInForeground", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kServiceWorkerImportedScriptUpdateCheck{
    "ServiceWorkerImportedScriptUpdateCheck",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables reading a subresource's body data and side data in parallel.
const base::Feature kServiceWorkerParallelSideDataReading{
    "ServiceWorkerParallelSideDataReading", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceWorkerAggressiveCodeCache{
    "ServiceWorkerAggressiveCodeCache", base::FEATURE_DISABLED_BY_DEFAULT};

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
const base::Feature kServiceWorkerUpdateDelay{
    "ServiceWorkerUpdateDelay", base::FEATURE_DISABLED_BY_DEFAULT};

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopInBackground {
  "stop-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Freeze scheduler task queues in background on network idle.
// This feature only works if stop-in-background is enabled.
const base::Feature kFreezeBackgroundTabOnNetworkIdle{
    "freeze-background-tab-on-network-idle", base::FEATURE_DISABLED_BY_DEFAULT};

// Freeze non-timer task queues in background, after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopNonTimersInBackground{
    "stop-non-timers-in-background", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable text snippets in URL fragments. https://crbug.com/919204.
const base::Feature kTextFragmentAnchor{"TextFragmentAnchor",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the site isolated Wasm code cache that is keyed on the resource URL
// and the origin lock of the renderer that is requesting the resource. When
// this flag is enabled, content/GeneratedCodeCache handles code cache requests.
const base::Feature kWasmCodeCache = {"WasmCodeCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Writable files and native file system access. https://crbug.com/853326
const base::Feature kNativeFileSystemAPI{"NativeFileSystemAPI",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// File handling integration. https://crbug.com/829689
const base::Feature kFileHandlingAPI{"FileHandlingAPI",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Allows for synchronous XHR requests during page dismissal
const base::Feature kForbidSyncXHRInPageDismissal{
    "ForbidSyncXHRInPageDismissal", base::FEATURE_DISABLED_BY_DEFAULT};

const char kMixedContentAutoupgradeModeParamName[] = "mode";
const char kMixedContentAutoupgradeModeBlockable[] = "blockable";
const char kMixedContentAutoupgradeModeOptionallyBlockable[] =
    "optionally-blockable";

// Decodes lossy WebP images to YUV instead of RGBX and stores in this format
// in the image decode cache. See crbug.com/900264 for details on the feature.
const base::Feature kDecodeLossyWebPImagesToYUV{
    "DecodeLossyWebPImagesToYUV", base::FEATURE_DISABLED_BY_DEFAULT};

// Use accelerated canvases whenever possible see https://crbug.com/909937
const base::Feature kAlwaysAccelerateCanvas{"AlwaysAccelerateCanvas",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
const base::Feature kWebFontsCacheAwareTimeoutAdaption {
  "WebFontsCacheAwareTimeoutAdaption",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enabled to block programmatic focus in subframes when not triggered by user
// activation (see htpps://crbug.com/954349).
const base::Feature kBlockingFocusWithoutUserActivation{
    "BlockingFocusWithoutUserActivation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAudioWorkletRealtimeThread{
    "AudioWorkletRealtimeThread", base::FEATURE_DISABLED_BY_DEFAULT};

// Use scroll gestures for scrollbar scrolls (see https://crbug.com/954007).
const base::Feature kScrollbarInjectScrollGestures{
    "ScrollbarInjectScrollGestures", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsOffMainThreadSharedWorkerScriptFetchEnabled() {
  // Off-the-main-thread shared worker script fetch depends on PlzSharedWorker
  // (NetworkService).
  return base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         base::FeatureList::IsEnabled(
             features::kOffMainThreadSharedWorkerScriptFetch);
}

bool IsPlzDedicatedWorkerEnabled() {
  // PlzDedicatedWorker depends on off-the-main-thread dedicated worker script
  // fetch and NetworkService.
#if DCHECK_IS_ON()
  if (base::FeatureList::IsEnabled(features::kPlzDedicatedWorker)) {
    DCHECK(base::FeatureList::IsEnabled(
        features::kOffMainThreadDedicatedWorkerScriptFetch))
        << "PlzDedicatedWorker is enabled but "
        << "OffMainThreadDedicatedWorkerScriptFetch isn't. PlzDedicatedWorker "
        << "requires OffMainThreadDedicatedWorkerScriptFetch.";
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService))
        << "PlzDedicatedWorker is enabled but NetworkService isn't. "
        << "PlzDedicatedWorker requires NetworkService.";
  }
#endif  // DCHECK_IS_ON()
  return base::FeatureList::IsEnabled(
             features::kOffMainThreadDedicatedWorkerScriptFetch) &&
         base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         base::FeatureList::IsEnabled(features::kPlzDedicatedWorker);
}

const base::Feature kCanvasAlwaysDeferral{"CanvasAlwaysDeferral",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
