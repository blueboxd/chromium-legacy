// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHolding;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kFreezeUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature kImplicitRootScroller;
BLINK_COMMON_EXPORT extern const base::Feature kCSSOMViewScrollCoordinates;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature
    kAssumeOverlapAfterFixedOrStickyPosition;
BLINK_COMMON_EXPORT extern const base::Feature kJSONModules;
BLINK_COMMON_EXPORT extern const base::Feature kTopLevelAwait;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature kPortalsCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRawClipboard;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGetDisplayMedia;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kV8OptimizeWorkersForPerformance;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcHideLocalIpsWithMdns;

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcH264WithOpenH264FFmpeg;
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStopNonTimersInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kStorageAccessAPI;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kFontAccess;
BLINK_COMMON_EXPORT extern const base::Feature kNativeFileSystemAPI;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingAPI;
BLINK_COMMON_EXPORT extern const base::Feature kAllowSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchPrivacyChanges;

BLINK_COMMON_EXPORT extern const base::Feature kWebComponentsV0Enabled;

BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeParamName[];
BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeAllPassive[];

BLINK_COMMON_EXPORT extern const base::Feature kDecodeJpeg420ImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingFocusWithoutUserActivation;

BLINK_COMMON_EXPORT extern const base::Feature kAudioWorkletRealtimeThread;

BLINK_COMMON_EXPORT extern const base::Feature kLightweightNoStatePrefetch;
BLINK_COMMON_EXPORT extern const base::Feature
    kLightweightNoStatePrefetch_FetchFonts;

BLINK_COMMON_EXPORT extern const base::Feature kSaveDataImgSrcset;

BLINK_COMMON_EXPORT extern const base::Feature kForceWebContentsDarkMode;
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkImageBehavior>
    kForceDarkImageBehaviorParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkTextLightnessThresholdParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkBackgroundLightnessThresholdParam;

// Returns true when PlzDedicatedWorker is enabled.
BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcUseMinMaxVEADimensions;

// Blink garbage collection.
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapCompaction;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentMarking;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentSweeping;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapIncrementalMarking;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkHeapIncrementalMarkingStress;

BLINK_COMMON_EXPORT extern const base::Feature
    kVerifyHTMLFetchedFromAppCacheBeforeDelay;

BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkCompositorUseDisplayThreadPriority;

BLINK_COMMON_EXPORT extern const base::Feature
    kIgnoreCrossOriginWindowWhenNamedAccessOnWindow;

BLINK_COMMON_EXPORT extern const base::Feature
    kLowerJavaScriptPriorityWhenForceDeferred;

BLINK_COMMON_EXPORT extern const base::Feature kDisableForceDeferInChildFrames;

BLINK_COMMON_EXPORT extern const base::Feature kARIAAnnotations;

BLINK_COMMON_EXPORT extern const base::Feature kCompositeCrossOriginIframes;

BLINK_COMMON_EXPORT extern const base::Feature kSubresourceRedirect;

BLINK_COMMON_EXPORT extern const base::Feature kSetLowPriorityForBeacon;

BLINK_COMMON_EXPORT extern const base::Feature kCacheStorageCodeCacheHintHeader;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT extern const base::Feature kDispatchBeforeUnloadOnFreeze;

BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dImageChromium;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dSwapChain;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyWebGLSwapChain;

BLINK_COMMON_EXPORT extern const base::Feature
    kForceExtraRenderingToTrackStickyFrame;

BLINK_COMMON_EXPORT extern const base::Feature
    kCSSReducedFontLoadingInvalidations;

BLINK_COMMON_EXPORT extern const base::Feature kDiscardCodeCacheAfterFirstUse;

BLINK_COMMON_EXPORT extern const base::Feature
    kSuppressContentTypeForBeaconMadeWithArrayBufferView;

BLINK_COMMON_EXPORT extern const base::Feature kBlockHTMLParserOnStyleSheets;

BLINK_COMMON_EXPORT extern const base::Feature kFontPreloadingDelaysRendering;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFontPreloadingDelaysRenderingParam;

BLINK_COMMON_EXPORT extern const base::Feature kFlexNG;

BLINK_COMMON_EXPORT extern const base::Feature kKeepScriptResourceAlive;

BLINK_COMMON_EXPORT extern const base::Feature kAppCache;

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
