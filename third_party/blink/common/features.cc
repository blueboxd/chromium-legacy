// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
const base::Feature kBlockingDownloadsInAdFrameWithoutUserActivation{
    "BlockingDownloadsInAdFrameWithoutUserActivation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable defer commits a bit to avoid flash.
const base::Feature kAvoidFlashBetweenNavigation{
    "AvoidFlashBetweenNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the user-specified viewport restriction for GPU Rasterization on
// mobile. See https://crbug.com/899399
const base::Feature kEnableGpuRasterizationViewportRestriction{
    "EnableGpuRasterizationViewportRestriction",
    base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enable a new CSS property called backdrop-filter.
const base::Feature kCSSBackdropFilter{"CSSBackdropFilter",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Display Locking JavaScript APIs.
const base::Feature kDisplayLocking{"DisplayLocking",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable applying rounded corner masks via a GL shader rather than
// a mask layer.
const base::Feature kFastBorderRadius {
  "FastBorderRadius",
#if defined(OS_MACOSX)
      // A FastBorderRadiusMac experiment is rolling out separately for that
      // platform, due to complications with CALayer overlay optimizations.
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded. Additionally, navigation
// predictor may preconnect/prefetch to resources/origins to make the
// future navigations faster.
const base::Feature kNavigationPredictor {
  "NavigationPredictor",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Start service workers on a background thread.
// https://crbug.com/692909
const base::Feature kOffMainThreadServiceWorkerStartup{
    "OffMainThreadServiceWorkerStartup", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables HW VP8 encoding on Android.
const base::Feature kWebRtcHWVP8Encoding {
  "WebRtcHWVP8Encoding",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables HW VP9 encoding on Android.
const base::Feature kWebRtcHWVP9Encoding {
  "WebRtcHWVP9Encoding",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables HW H264 encoding on Android.
const base::Feature kWebRtcHWH264Encoding{"WebRtcHWH264Encoding",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables negotiation of experimental multiplex codec in SDP.
const base::Feature kWebRtcMultiplexCodec{"WebRTC-MultiplexCodec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
const base::Feature kWebRtcHideLocalIpsWithMdns{
    "WebRtcHideLocalIpsWithMdns", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
const base::Feature kWebRtcH264WithOpenH264FFmpeg{
    "WebRTC-H264WithOpenH264FFmpeg", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

const base::Feature kServiceWorkerIsolateInForeground{
    "ServiceWorkerIsolateInForeground", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceWorkerImportedScriptUpdateCheck{
    "ServiceWorkerImportedScriptUpdateCheck", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enable the Storage Access API. https://crbug.com/989663.
const base::Feature kStorageAccessAPI{"StorageAccessAPI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

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
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// File handling integration. https://crbug.com/829689
const base::Feature kFileHandlingAPI{"FileHandlingAPI",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Allows for synchronous XHR requests during page dismissal
const base::Feature kAllowSyncXHRInPageDismissal{
    "AllowSyncXHRInPageDismissal", base::FEATURE_DISABLED_BY_DEFAULT};

// Prefetch request properties are updated to be privacy-preserving. See
// crbug.com/988956.
const base::Feature kPrefetchPrivacyChanges{"PrefetchPrivacyChanges",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const char kMixedContentAutoupgradeModeParamName[] = "mode";
const char kMixedContentAutoupgradeModeBlockable[] = "blockable";
const char kMixedContentAutoupgradeModeOptionallyBlockable[] =
    "optionally-blockable";

// Decodes jpeg 4:2:0 formatted images to YUV instead of RGBX and stores in this
// format in the image decode cache. See crbug.com/919627 for details on the
// feature.
const base::Feature kDecodeJpeg420ImagesToYUV{
    "DecodeJpeg420ImagesToYUV", base::FEATURE_DISABLED_BY_DEFAULT};

// Decodes lossy WebP images to YUV instead of RGBX and stores in this format
// in the image decode cache. See crbug.com/900264 for details on the feature.
const base::Feature kDecodeLossyWebPImagesToYUV{
    "DecodeLossyWebPImagesToYUV", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables usage of render frame observer as the receiver of the resource
// loading hints in the render process.
// https://crbug.com/891328.
const base::Feature kSendPreviewsLoadingHintsBeforeCommit{
    "SendPreviewsLoadingHintsBeforeCommit", base::FEATURE_ENABLED_BY_DEFAULT};

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

// A feature to reduce the set of resources fetched by No-State Prefetch.
const base::Feature kLightweightNoStatePrefetch{
    "LightweightNoStatePrefetch", base::FEATURE_DISABLED_BY_DEFAULT};

// Use scroll gestures for scrollbar scrolls (see https://crbug.com/954007).
const base::Feature kScrollbarInjectScrollGestures{
    "ScrollbarInjectScrollGestures", base::FEATURE_ENABLED_BY_DEFAULT};

// Automatically convert light-themed pages to use a Blink-generated dark theme
const base::Feature kForceWebContentsDarkMode{
    "WebContentsForceDark", base::FEATURE_DISABLED_BY_DEFAULT};

// Which algorithm should be used for color inversion?
const base::FeatureParam<ForceDarkInversionMethod>::Option
    forcedark_inversion_method_options[] = {
        {ForceDarkInversionMethod::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkInversionMethod::kHslBased, "hsl_based"},
        {ForceDarkInversionMethod::kCielabBased, "cielab_based"},
        {ForceDarkInversionMethod::kRgbBased, "rgb_based"}};

const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam{&kForceWebContentsDarkMode,
                                   "inversion_method",
                                   ForceDarkInversionMethod::kUseBlinkSettings,
                                   &forcedark_inversion_method_options};

// Should images be inverted?
const base::FeatureParam<ForceDarkImageBehavior>::Option
    forcedark_image_behavior_options[] = {
        {ForceDarkImageBehavior::kUseBlinkSettings,
         "use_blink_settings_for_images"},
        {ForceDarkImageBehavior::kInvertNone, "none"},
        {ForceDarkImageBehavior::kInvertSelectively, "selective"}};

const base::FeatureParam<ForceDarkImageBehavior> kForceDarkImageBehaviorParam{
    &kForceWebContentsDarkMode, "image_behavior",
    ForceDarkImageBehavior::kUseBlinkSettings,
    &forcedark_image_behavior_options};

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 256 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkTextLightnessThresholdParam{
    &kForceWebContentsDarkMode, "text_lightness_threshold", -1};

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 256 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkBackgroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "background_lightness_threshold", -1};

const base::Feature kCanvasAlwaysDeferral{"CanvasAlwaysDeferral",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Use the new C++ implementation of WHATWG Streams. See
// https://crbug.com/977500.
const base::Feature kStreamsNative{"StreamsNative",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Blink garbage collection.
// Enables compaction of backing stores on Blink's heap.
const base::Feature kBlinkHeapCompaction{"BlinkHeapCompaction",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
// Enables concurrently marking Blink's heap.
const base::Feature kBlinkHeapConcurrentMarking{
    "BlinkHeapConcurrentMarking", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables concurrently sweeping Blink's heap.
const base::Feature kBlinkHeapConcurrentSweeping{
    "BlinkHeapConcurrentSweeping", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables incrementally marking Blink's heap.
const base::Feature kBlinkHeapIncrementalMarking{
    "BlinkHeapIncrementalMarking", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables a marking stress mode that schedules more garbage collections and
// also adds additional verification passes.
const base::Feature kBlinkHeapIncrementalMarkingStress{
    "BlinkHeapIncrementalMarkingStress", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a delay before BufferingBytesConsumer begins reading from its
// underlying consumer when instantiated with CreateWithDelay().
const base::Feature kBufferingBytesConsumerDelay{
    "BufferingBytesConsumerDelay", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<int> kBufferingBytesConsumerDelayMilliseconds{
    &kBufferingBytesConsumerDelay, "milliseconds", 50};

// Enables removing AppCache delays when triggering requests when the HTML was
// not fetched from AppCache.
const base::Feature kVerifyHTMLFetchedFromAppCacheBeforeDelay{
    "VerifyHTMLFetchedFromAppCacheBeforeDelay",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we use ThreadPriority::DISPLAY for renderer
// compositor & IO threads.
const base::Feature kBlinkCompositorUseDisplayThreadPriority {
  "BlinkCompositorUseDisplayThreadPriority",
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Ignores cross origin windows in the named property interceptor of Window.
// https://crbug.com/538562
const base::Feature kIgnoreCrossOriginWindowWhenNamedAccessOnWindow{
    "IgnoreCrossOriginWindowWhenNamedAccessOnWindow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, loading priority of JavaScript requests is lowered when they
// are force deferred by the intervention.
const base::Feature kLowerJavaScriptPriorityWhenForceDeferred{
    "LowerJavaScriptPriorityWhenForceDeferred",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHtmlImportsRequestInitiatorLock{
    "HtmlImportsRequestInitiatorLock", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
