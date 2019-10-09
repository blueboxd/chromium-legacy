// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/task/task_features.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using blink::WebRuntimeFeatures;

namespace {

// Sets blink runtime features for specific platforms.
// This should be a last resort vs runtime_enabled_features.json5.
void SetRuntimeFeatureDefaultsForPlatform(
    const base::CommandLine& command_line) {
  // Please consider setting up feature defaults for different platforms
  // in runtime_enabled_features.json5 instead of here
#if defined(USE_AURA)
  WebRuntimeFeatures::EnableCompositedSelectionUpdate(true);
#endif
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    WebRuntimeFeatures::EnableWebBluetooth(true);
#endif

#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
  if (command_line.HasSwitch(switches::kEnableWebGL2ComputeContext)) {
    WebRuntimeFeatures::EnableWebGL2ComputeContext(true);
  }
#endif

#if defined(OS_MACOSX)
  const bool enable_canvas_2d_image_chromium =
      command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisable2dCanvasImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kCanvas2DImageChromium);
#else
  constexpr bool enable_canvas_2d_image_chromium = false;
#endif
  WebRuntimeFeatures::EnableCanvas2dImageChromium(
      enable_canvas_2d_image_chromium);

#if defined(OS_MACOSX)
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kWebGLImageChromium);
#else
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::EnableWebGLImageChromium(enable_web_gl_image_chromium);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::EnableMediaSession(false);
#endif

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnableWebNfc(
      base::FeatureList::IsEnabled(features::kWebNfc));
#endif

#if defined(OS_ANDROID)
  // APIs for Web Authentication are not available prior to N.
  WebRuntimeFeatures::EnableWebAuth(
      base::FeatureList::IsEnabled(features::kWebAuth) &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_NOUGAT);
#else
  WebRuntimeFeatures::EnableWebAuth(
      base::FeatureList::IsEnabled(features::kWebAuth));
#endif

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnablePictureInPictureAPI(
      base::FeatureList::IsEnabled(media::kPictureInPictureAPI));
#endif

#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    // Display Cutout is limited to Android P+.
    WebRuntimeFeatures::EnableDisplayCutoutAPI(true);
  }
#endif

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnableMediaControlsExpandGesture(
      base::FeatureList::IsEnabled(media::kMediaControlsExpandGesture));
#endif
}

// Sets blink runtime features that are either directly
// controlled by Chromium base::Feature or are overridden
// by base::Feature states.
void SetRuntimeFeaturesFromChromiumFeatures() {
  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    WebRuntimeFeatures::EnableWebUsb(false);

  if (base::FeatureList::IsEnabled(
          blink::features::kBlockingFocusWithoutUserActivation)) {
    WebRuntimeFeatures::EnableBlockingFocusWithoutUserActivation(true);
  }

  if (!base::FeatureList::IsEnabled(features::kNotificationContentImage))
    WebRuntimeFeatures::EnableNotificationContentImage(false);

  WebRuntimeFeatures::EnableSharedArrayBuffer(
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer) ||
      base::FeatureList::IsEnabled(features::kWebAssemblyThreads));

  WebRuntimeFeatures::EnableReducedReferrerGranularity(
      base::FeatureList::IsEnabled(features::kReducedReferrerGranularity));

  if (base::FeatureList::IsEnabled(features::kPeriodicBackgroundSync))
    WebRuntimeFeatures::EnablePeriodicBackgroundSync(true);

  if (base::FeatureList::IsEnabled(features::kWebXr))
    WebRuntimeFeatures::EnableWebXR(true);

  if (base::FeatureList::IsEnabled(features::kWebXrArDOMOverlay))
    WebRuntimeFeatures::EnableWebXRARDOMOverlay(true);

  if (base::FeatureList::IsEnabled(features::kWebXrArModule))
    WebRuntimeFeatures::EnableWebXRARModule(true);

  if (base::FeatureList::IsEnabled(features::kWebXrHitTest))
    WebRuntimeFeatures::EnableWebXRHitTest(true);

  if (base::FeatureList::IsEnabled(features::kWebXrAnchors))
    WebRuntimeFeatures::EnableWebXRAnchors(true);

  if (base::FeatureList::IsEnabled(features::kWebXrPlaneDetection))
    WebRuntimeFeatures::EnableWebXRPlaneDetection(true);

  WebRuntimeFeatures::EnableFetchMetadata(
      base::FeatureList::IsEnabled(network::features::kFetchMetadata));
  WebRuntimeFeatures::EnableFetchMetadataDestination(
      base::FeatureList::IsEnabled(
          network::features::kFetchMetadataDestination));

  WebRuntimeFeatures::EnableUserActivationPostMessageTransfer(
      base::FeatureList::IsEnabled(
          features::kUserActivationPostMessageTransfer));

  WebRuntimeFeatures::EnableUserActivationSameOriginVisibility(
      base::FeatureList::IsEnabled(
          features::kUserActivationSameOriginVisibility));

  WebRuntimeFeatures::EnableUserActivationV2(
      base::FeatureList::IsEnabled(features::kUserActivationV2));

  if (base::FeatureList::IsEnabled(features::kScrollAnchorSerialization))
    WebRuntimeFeatures::EnableScrollAnchorSerialization(true);

  WebRuntimeFeatures::EnableFeatureFromString(
      "CSSBackdropFilter",
      base::FeatureList::IsEnabled(blink::features::kCSSBackdropFilter));

  WebRuntimeFeatures::EnableFeatureFromString(
      "FastBorderRadius",
      base::FeatureList::IsEnabled(blink::features::kFastBorderRadius));

  WebRuntimeFeatures::EnablePassiveDocumentEventListeners(
      base::FeatureList::IsEnabled(features::kPassiveDocumentEventListeners));

  WebRuntimeFeatures::EnablePassiveDocumentWheelEventListeners(
      base::FeatureList::IsEnabled(
          features::kPassiveDocumentWheelEventListeners));

  WebRuntimeFeatures::EnableFeatureFromString(
      "FontSrcLocalMatching",
      base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));

  WebRuntimeFeatures::EnableFeatureFromString(
      "LegacyWindowsDWriteFontFallback",
      base::FeatureList::IsEnabled(features::kLegacyWindowsDWriteFontFallback));

  WebRuntimeFeatures::EnableExpensiveBackgroundTimerThrottling(
      base::FeatureList::IsEnabled(
          features::kExpensiveBackgroundTimerThrottling));

  WebRuntimeFeatures::EnableTimerThrottlingForHiddenFrames(
      base::FeatureList::IsEnabled(features::kTimerThrottlingForHiddenFrames));

  if (base::FeatureList::IsEnabled(
          features::kSendBeaconThrowForBlobWithNonSimpleType))
    WebRuntimeFeatures::EnableSendBeaconThrowForBlobWithNonSimpleType(true);

  WebRuntimeFeatures::EnablePaymentRequest(
      base::FeatureList::IsEnabled(features::kWebPayments));

  if (base::FeatureList::IsEnabled(features::kServiceWorkerPaymentApps))
    WebRuntimeFeatures::EnablePaymentApp(true);

  if (base::FeatureList::IsEnabled(features::kCompositorTouchAction))
    WebRuntimeFeatures::EnableCompositorTouchAction(true);

  if (base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses))
    WebRuntimeFeatures::EnableGenericSensorExtraClasses(true);

  if (base::FeatureList::IsEnabled(
          network::features::kBlockNonSecureExternalRequests)) {
    WebRuntimeFeatures::EnableFeatureFromString("AddressSpace", true);
  }

  WebRuntimeFeatures::EnableMediaCastOverlayButton(
      base::FeatureList::IsEnabled(media::kMediaCastOverlayButton));

  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources)) {
    WebRuntimeFeatures::EnableFeatureFromString("BlockCredentialedSubresources",
                                                false);
  }

  WebRuntimeFeatures::EnableFeatureFromString(
      "AllowContentInitiatedDataUrlNavigations",
      base::FeatureList::IsEnabled(
          features::kAllowContentInitiatedDataUrlNavigations));

  WebRuntimeFeatures::EnableResourceLoadScheduler(
      base::FeatureList::IsEnabled(features::kResourceLoadScheduler));

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleAll))
    WebRuntimeFeatures::EnableBuiltInModuleAll(true);

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleInfra))
    WebRuntimeFeatures::EnableBuiltInModuleInfra(true);

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleKvStorage))
    WebRuntimeFeatures::EnableBuiltInModuleKvStorage(true);

  WebRuntimeFeatures::EnableFeatureFromString(
      "LayoutNG", base::FeatureList::IsEnabled(blink::features::kLayoutNG));

  WebRuntimeFeatures::EnableLazyInitializeMediaControls(
      base::FeatureList::IsEnabled(features::kLazyInitializeMediaControls));

  WebRuntimeFeatures::EnableMediaEngagementBypassAutoplayPolicies(
      base::FeatureList::IsEnabled(
          media::kMediaEngagementBypassAutoplayPolicies));

  WebRuntimeFeatures::EnableOverflowIconsForMediaControls(
      base::FeatureList::IsEnabled(media::kOverflowIconsForMediaControls));

  WebRuntimeFeatures::EnableAllowActivationDelegationAttr(
      base::FeatureList::IsEnabled(features::kAllowActivationDelegationAttr));

  WebRuntimeFeatures::EnableScriptStreamingOnPreload(
      base::FeatureList::IsEnabled(features::kScriptStreamingOnPreload));

  WebRuntimeFeatures::EnableMergeBlockingNonBlockingPools(
      base::FeatureList::IsEnabled(base::kMergeBlockingNonBlockingPools));

  WebRuntimeFeatures::EnableLazyFrameLoading(
      base::FeatureList::IsEnabled(features::kLazyFrameLoading));
  WebRuntimeFeatures::EnableLazyFrameVisibleLoadTimeMetrics(
      base::FeatureList::IsEnabled(features::kLazyFrameVisibleLoadTimeMetrics));
  WebRuntimeFeatures::EnableLazyImageLoading(
      base::FeatureList::IsEnabled(features::kLazyImageLoading));
  WebRuntimeFeatures::EnableLazyImageVisibleLoadTimeMetrics(
      base::FeatureList::IsEnabled(features::kLazyImageVisibleLoadTimeMetrics));

  WebRuntimeFeatures::EnablePictureInPicture(
      base::FeatureList::IsEnabled(media::kPictureInPicture));

  WebRuntimeFeatures::EnableCacheInlineScriptCode(
      base::FeatureList::IsEnabled(features::kCacheInlineScriptCode));

  WebRuntimeFeatures::EnableWasmCodeCache(
      base::FeatureList::IsEnabled(blink::features::kWasmCodeCache));

  if (base::FeatureList::IsEnabled(
          features::kExperimentalProductivityFeatures)) {
    WebRuntimeFeatures::EnableExperimentalProductivityFeatures(true);
  }

  if (base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox))
    WebRuntimeFeatures::EnableFeaturePolicyForSandbox(true);

  WebRuntimeFeatures::EnableAccessibilityExposeARIAAnnotations(
      base::FeatureList::IsEnabled(
          features::kEnableAccessibilityExposeARIAAnnotations));

  WebRuntimeFeatures::EnableAccessibilityExposeDisplayNone(
      base::FeatureList::IsEnabled(
          features::kEnableAccessibilityExposeDisplayNone));

  if (base::FeatureList::IsEnabled(
          blink::features::kAllowSyncXHRInPageDismissal)) {
    WebRuntimeFeatures::EnableAllowSyncXHRInPageDismissal(true);
  }

  WebRuntimeFeatures::EnableAutoplayIgnoresWebAudio(
      base::FeatureList::IsEnabled(media::kAutoplayIgnoreWebAudio));

  WebRuntimeFeatures::EnablePortals(
      base::FeatureList::IsEnabled(blink::features::kPortals));

  WebRuntimeFeatures::EnableImplicitRootScroller(
      base::FeatureList::IsEnabled(blink::features::kImplicitRootScroller));

  WebRuntimeFeatures::EnableTextFragmentAnchor(
      base::FeatureList::IsEnabled(blink::features::kTextFragmentAnchor));

  if (!base::FeatureList::IsEnabled(features::kBackgroundFetch))
    WebRuntimeFeatures::EnableBackgroundFetch(false);

  WebRuntimeFeatures::EnableUpdateHoverAtBeginFrame(
      base::FeatureList::IsEnabled(features::kUpdateHoverAtBeginFrame));

  WebRuntimeFeatures::EnableForcedColors(
      base::FeatureList::IsEnabled(features::kForcedColors));

  WebRuntimeFeatures::EnableFractionalScrollOffsets(
      base::FeatureList::IsEnabled(features::kFractionalScrollOffsets));

  WebRuntimeFeatures::EnableGetDisplayMedia(
      base::FeatureList::IsEnabled(blink::features::kRTCGetDisplayMedia));

  WebRuntimeFeatures::EnableMimeHandlerViewInCrossProcessFrame(
      base::FeatureList::IsEnabled(
          features::kMimeHandlerViewInCrossProcessFrame));

  WebRuntimeFeatures::EnableFallbackCursorMode(
      base::FeatureList::IsEnabled(features::kFallbackCursorMode));

  if (base::FeatureList::IsEnabled(features::kUserAgentClientHint))
    WebRuntimeFeatures::EnableFeatureFromString("UserAgentClientHint", true);

  WebRuntimeFeatures::EnableSignedExchangePrefetchCacheForNavigations(
      base::FeatureList::IsEnabled(
          features::kSignedExchangePrefetchCacheForNavigations));
  WebRuntimeFeatures::EnableSignedExchangeSubresourcePrefetch(
      base::FeatureList::IsEnabled(
          features::kSignedExchangeSubresourcePrefetch));

  if (!base::FeatureList::IsEnabled(features::kIdleDetection))
    WebRuntimeFeatures::EnableIdleDetection(false);

  WebRuntimeFeatures::EnableSkipTouchEventFilter(
      base::FeatureList::IsEnabled(features::kSkipTouchEventFilter));

  if (!base::FeatureList::IsEnabled(features::kSmsReceiver))
    WebRuntimeFeatures::EnableSmsReceiver(false);

  WebRuntimeFeatures::EnableDisplayLocking(
      base::FeatureList::IsEnabled(blink::features::kDisplayLocking));

  if (base::FeatureList::IsEnabled(
          blink::features::kAudioWorkletRealtimeThread)) {
    WebRuntimeFeatures::EnableFeatureFromString("AudioWorkletRealtimeThread",
                                                true);
  }

  WebRuntimeFeatures::EnableConsolidatedMovementXY(
      base::FeatureList::IsEnabled(features::kConsolidatedMovementXY));

  WebRuntimeFeatures::EnableCooperativeScheduling(
      base::FeatureList::IsEnabled(features::kCooperativeScheduling));

  WebRuntimeFeatures::EnableMouseSubframeNoImplicitCapture(
      base::FeatureList::IsEnabled(features::kMouseSubframeNoImplicitCapture));

  if (base::FeatureList::IsEnabled(features::kTrustedDOMTypes))
    WebRuntimeFeatures::EnableFeatureFromString("TrustedDOMTypes", true);

  WebRuntimeFeatures::EnableBackForwardCache(
      base::FeatureList::IsEnabled(features::kBackForwardCache));

  if (base::FeatureList::IsEnabled(features::kCookieDeprecationMessages))
    WebRuntimeFeatures::EnableCookieDeprecationMessages(true);

  if (base::FeatureList::IsEnabled(net::features::kSameSiteByDefaultCookies))
    WebRuntimeFeatures::EnableSameSiteByDefaultCookies(true);

  if (base::FeatureList::IsEnabled(
          net::features::kCookiesWithoutSameSiteMustBeSecure)) {
    WebRuntimeFeatures::EnableCookiesWithoutSameSiteMustBeSecure(true);
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kIgnoreCrossOriginWindowWhenNamedAccessOnWindow)) {
    WebRuntimeFeatures::EnableFeatureFromString(
        "IgnoreCrossOriginWindowWhenNamedAccessOnWindow", true);
  }

  if (base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    WebRuntimeFeatures::EnableFeatureFromString("StorageAccessAPI", true);
  }

  if (base::FeatureList::IsEnabled(features::kPointerLockOptions)) {
    WebRuntimeFeatures::EnablePointerLockOptions(true);
  }

  WebRuntimeFeatures::EnableDocumentPolicy(
      base::FeatureList::IsEnabled(features::kDocumentPolicy));
}

// Sets blink runtime features controlled by command line switches.
void SetRuntimeFeaturesFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::EnableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::EnableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::EnablePushMessaging(false);
  }

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::EnableSharedWorker(false);

  if (command_line.HasSwitch(switches::kDisableSpeechAPI)) {
    WebRuntimeFeatures::EnableScriptedSpeechRecognition(false);
    WebRuntimeFeatures::EnableScriptedSpeechSynthesis(false);
  }

  if (command_line.HasSwitch(switches::kDisableSpeechSynthesisAPI)) {
    WebRuntimeFeatures::EnableScriptedSpeechSynthesis(false);
  }

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::EnableFileSystem(false);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::EnableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableAutomation) ||
      command_line.HasSwitch(switches::kHeadless) ||
      command_line.HasSwitch(switches::kRemoteDebuggingPipe) ||
      command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    WebRuntimeFeatures::EnableAutomationControlled(true);
  }

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::ForceOverlayFullscreenVideo(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::EnablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnablePrintBrowser))
    WebRuntimeFeatures::EnablePrintBrowser(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformationDownlinkMax))
    WebRuntimeFeatures::EnableNetInfoDownlinkMax(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::EnablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  else
    WebRuntimeFeatures::EnableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableUnsafeWebGPU))
    WebRuntimeFeatures::EnableWebGPU(true);

  if (command_line.HasSwitch(switches::kEnableWebVR))
    WebRuntimeFeatures::EnableWebVR(true);

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::EnablePresentationAPI(false);

  if (command_line.HasSwitch(switches::kDisableRemotePlaybackAPI))
    WebRuntimeFeatures::EnableRemotePlaybackAPI(false);

  if (command_line.HasSwitch(switches::kDisableBackgroundTimerThrottling))
    WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(false);

  if (command_line.HasSwitch(switches::kEnableAccessibilityObjectModel))
    WebRuntimeFeatures::EnableAccessibilityObjectModel(true);

  if (command_line.HasSwitch(switches::kAllowSyncXHRInPageDismissal))
    WebRuntimeFeatures::EnableAllowSyncXHRInPageDismissal(true);
}

// Sets blink runtime features controlled by FieldTrial parameter values.
void SetRuntimeFeaturesFromFieldTrialParams() {
  // Automatic lazy frame loading by default is enabled and restricted to users
  // with Lite Mode (aka Data Saver) turned on. Note that in practice, this also
  // restricts automatic lazy loading by default to Android, since Lite Mode is
  // only accessible through UI on Android.
  WebRuntimeFeatures::EnableAutomaticLazyFrameLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading, "automatic-lazy-load-frames-enabled",
          true));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyFrameLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading,
          "restrict-lazy-load-frames-to-data-saver-only", true));
  WebRuntimeFeatures::EnableAutoLazyLoadOnReloads(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading, "enable-lazy-load-on-reload", false));

  // Automatic lazy image loading by default is enabled and restricted to users
  // with Lite Mode (aka Data Saver) turned on. Note that in practice, this also
  // restricts automatic lazy loading by default to Android, since Lite Mode is
  // only accessible through UI on Android.
  WebRuntimeFeatures::EnableAutomaticLazyImageLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading, "automatic-lazy-load-images-enabled",
          true));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyImageLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading,
          "restrict-lazy-load-images-to-data-saver-only", true));
  WebRuntimeFeatures::EnableLazyImageLoadingMetadataFetch(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading, "enable-lazy-load-images-metadata-fetch",
          false));
}

// Sets blink runtime features that depend on a combination
// of args rather than a single check of base::Feature or switch.
// This can be a combination of both or custom checking logic
// not covered by other functions. In short, this should be used
// as a last resort.
void SetCustomizedRuntimeFeaturesFromCombinedArgs(
    const base::CommandLine& command_line,
    bool enable_experimental_web_platform_features) {
  // CAUTION: Only add custom enabling logic here if it cannot
  // be covered by the other functions.

  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeJpeg420ImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeJpeg420ImagesToYUV(true);
  }
  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeLossyWebPImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeLossyWebPImagesToYUV(true);
  }

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::EnableOverlayScrollbars(true);
  if (network::features::ShouldEnableOutOfBlinkCors())
    WebRuntimeFeatures::EnableOutOfBlinkCors(true);
  WebRuntimeFeatures::EnableFormControlsRefresh(
      features::IsFormControlsRefreshEnabled());

  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          blink::features::kNativeFileSystemAPI.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    WebRuntimeFeatures::EnableFeatureFromString("NativeFileSystem", true);
  }
  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI) &&
      base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI)) {
    WebRuntimeFeatures::EnableFeatureFromString("FileHandling", true);
  }

  // This is a hack to get the tests passing as they require
  // these blink features to be enabled while they are disabled
  // by base::Feature controls earlier in code.
  // TODO(rodneyding): Investigate more on proper treatments of
  // these features.
  if (enable_experimental_web_platform_features) {
    WebRuntimeFeatures::EnableNetInfoDownlinkMax(true);
    WebRuntimeFeatures::EnableFetchMetadata(true);
    WebRuntimeFeatures::EnableFetchMetadataDestination(true);
    WebRuntimeFeatures::EnableFeatureFromString("CSSBackdropFilter", true);
    WebRuntimeFeatures::EnableFeatureFromString("FastBorderRadius", true);
    WebRuntimeFeatures::EnableDisplayLocking(true);
  }
}

}  // namespace

namespace content {

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  // Sets experimental features.
  bool enable_experimental_web_platform_features =
      command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  if (enable_experimental_web_platform_features)
    WebRuntimeFeatures::EnableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform(command_line);

  // Sets origin trial features.
  if (command_line.HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }

  SetRuntimeFeaturesFromChromiumFeatures();

  SetRuntimeFeaturesFromCommandLine(command_line);

  SetRuntimeFeaturesFromFieldTrialParams();

  SetCustomizedRuntimeFeaturesFromCombinedArgs(
      command_line, enable_experimental_web_platform_features);

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kEnableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kDisableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }
}

}  // namespace content
