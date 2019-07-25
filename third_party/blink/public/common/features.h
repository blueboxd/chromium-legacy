// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kAvoidFlashBetweenNavigation;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature
    kEnableGpuRasterizationViewportRestriction;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFirstContentfulPaintPlusPlus;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kFreezeUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature kImplicitRootScroller;
BLINK_COMMON_EXPORT extern const base::Feature kJankTrackingSweepLine;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkGenPropertyTrees;
BLINK_COMMON_EXPORT extern const base::Feature kCSSBackdropFilter;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature kFastBorderRadius;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kMojoBlobURLs;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature
    kOffMainThreadDedicatedWorkerScriptFetch;
BLINK_COMMON_EXPORT extern const base::Feature
    kOffMainThreadServiceWorkerScriptFetch;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGetDisplayMedia;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerIsolateInForeground;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerImportedScriptUpdateCheck;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerAggressiveCodeCache;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStopNonTimersInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kWasmCodeCache;
BLINK_COMMON_EXPORT extern const base::Feature kNativeFileSystemAPI;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingAPI;
BLINK_COMMON_EXPORT extern const base::Feature kForbidSyncXHRInPageDismissal;

BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeParamName[];
BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeBlockable[];
BLINK_COMMON_EXPORT extern const char
    kMixedContentAutoupgradeModeOptionallyBlockable[];

BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kAlwaysAccelerateCanvas;

BLINK_COMMON_EXPORT extern const base::Feature
    kSendPreviewsLoadingHintsBeforeCommit;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingFocusWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kScrollbarInjectScrollGestures;

BLINK_COMMON_EXPORT extern const base::Feature kAudioWorkletRealtimeThread;

BLINK_COMMON_EXPORT extern const base::Feature kLightweightNoStatePrefetch;

// Returns true when off-the-main-thread shared worker script fetch is enabled.
BLINK_COMMON_EXPORT bool IsOffMainThreadSharedWorkerScriptFetchEnabled();

// Returns true when PlzDedicatedWorker is enabled.
BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT extern const base::Feature kCanvasAlwaysDeferral;

BLINK_COMMON_EXPORT extern const base::Feature kStreamsNative;

// Blink garbage collection.
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapCompaction;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentMarking;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentSweeping;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapIncrementalMarking;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkHeapIncrementalMarkingStress;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapUnifiedGCScheduling;

BLINK_COMMON_EXPORT extern const base::Feature kBufferingBytesConsumerDelay;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kBufferingBytesConsumerDelayMilliseconds;
BLINK_COMMON_EXPORT extern const base::Feature
    kVerifyHTMLFetchedFromAppCacheBeforeDelay;

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
