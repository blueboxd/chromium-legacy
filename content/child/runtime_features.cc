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
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
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

void SetRuntimeFeatureDefaultsForPlatform() {
  // Please consider setting up feature defaults for different platforms
  // in runtime_enabled_features.json5 instead of here

#if defined(USE_AURA)
  WebRuntimeFeatures::EnableCompositedSelectionUpdate(true);
#endif

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    WebRuntimeFeatures::EnableWebBluetooth(true);
#endif
}

void SetIndividualRuntimeFeatures(
    const base::CommandLine& command_line,
    bool enable_experimental_web_platform_features) {
  WebRuntimeFeatures::EnableOriginTrials(
      base::FeatureList::IsEnabled(features::kOriginTrials));

  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    WebRuntimeFeatures::EnableWebUsb(false);

  WebRuntimeFeatures::EnableBlinkHeapIncrementalMarking(
      base::FeatureList::IsEnabled(features::kBlinkHeapIncrementalMarking));

  WebRuntimeFeatures::EnableBlinkHeapUnifiedGCScheduling(
      base::FeatureList::IsEnabled(features::kBlinkHeapUnifiedGCScheduling));

  if (base::FeatureList::IsEnabled(features::kBloatedRendererDetection))
    WebRuntimeFeatures::EnableBloatedRendererDetection(true);

  if (base::FeatureList::IsEnabled(
          blink::features::kBlockingFocusWithoutUserActivation)) {
    WebRuntimeFeatures::EnableBlockingFocusWithoutUserActivation(true);
  }

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::EnableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::EnableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::EnablePushMessaging(false);
  }

  if (!base::FeatureList::IsEnabled(features::kNotificationContentImage))
    WebRuntimeFeatures::EnableNotificationContentImage(false);

  WebRuntimeFeatures::EnableSharedArrayBuffer(
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer) ||
      base::FeatureList::IsEnabled(features::kWebAssemblyThreads));

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

  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeLossyWebPImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeLossyWebPImagesToYUV(true);
  }

#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
  if (command_line.HasSwitch(switches::kEnableWebGL2ComputeContext)) {
    WebRuntimeFeatures::EnableWebGL2ComputeContext(true);
  }
#endif

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::EnableWebGLDraftExtensions(true);

  if (command_line.HasSwitch(switches::kEnableAutomation) ||
      command_line.HasSwitch(switches::kHeadless)) {
    WebRuntimeFeatures::EnableAutomationControlled(true);
  }

  if (command_line.HasSwitch(switches::kEnableWebBluetoothScanning))
    WebRuntimeFeatures::EnableWebBluetoothScanning(true);

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

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::ForceOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::EnableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::EnablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnablePrintBrowser))
    WebRuntimeFeatures::EnablePrintBrowser(true);

  // TODO(yashard): Remove |enable_experimental_web_platform_features| flag
  // since the feature should have been enabled when it is set to experimental
  if (command_line.HasSwitch(switches::kEnableNetworkInformationDownlinkMax) ||
      enable_experimental_web_platform_features) {
    WebRuntimeFeatures::EnableNetInfoDownlinkMax(true);
  }

  WebRuntimeFeatures::EnableReducedReferrerGranularity(
      base::FeatureList::IsEnabled(features::kReducedReferrerGranularity));

  if (base::FeatureList::IsEnabled(features::kPeriodicBackgroundSync))
    WebRuntimeFeatures::EnablePeriodicBackgroundSync(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::EnablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  else
    WebRuntimeFeatures::EnableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableUnsafeWebGPU))
    WebRuntimeFeatures::EnableWebGPU(true);

  if (command_line.HasSwitch(switches::kEnableWebGLSwapChain))
    WebRuntimeFeatures::EnableWebGLSwapChain(true);

  if (command_line.HasSwitch(switches::kEnableWebVR))
    WebRuntimeFeatures::EnableWebVR(true);

  if (base::FeatureList::IsEnabled(features::kWebXr))
    WebRuntimeFeatures::EnableWebXR(true);

  if (base::FeatureList::IsEnabled(features::kWebXrHitTest))
    WebRuntimeFeatures::EnableWebXRHitTest(true);

  if (base::FeatureList::IsEnabled(features::kWebXrPlaneDetection))
    WebRuntimeFeatures::EnableWebXRPlaneDetection(true);

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::EnablePresentationAPI(false);

  if (command_line.HasSwitch(switches::kDisableRemotePlaybackAPI))
    WebRuntimeFeatures::EnableRemotePlaybackAPI(false);

  // TODO(yashard): Remove |enable_experimental_web_platform_features| flag
  // since the feature should have been enabled when it is set to experimental
  WebRuntimeFeatures::EnableFetchMetadata(
      base::FeatureList::IsEnabled(network::features::kFetchMetadata) ||
      enable_experimental_web_platform_features);
  WebRuntimeFeatures::EnableFetchMetadataDestination(
      base::FeatureList::IsEnabled(
          network::features::kFetchMetadataDestination) ||
      enable_experimental_web_platform_features);

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
      "BlinkGenPropertyTrees",
      base::FeatureList::IsEnabled(blink::features::kBlinkGenPropertyTrees) ||
          enable_experimental_web_platform_features);

  WebRuntimeFeatures::EnableFeatureFromString(
      "CSSBackdropFilter",
      base::FeatureList::IsEnabled(blink::features::kCSSBackdropFilter) ||
          enable_experimental_web_platform_features);

  WebRuntimeFeatures::EnableFeatureFromString(
      "FastBorderRadius",
      base::FeatureList::IsEnabled(blink::features::kFastBorderRadius) ||
          enable_experimental_web_platform_features);

  WebRuntimeFeatures::EnablePassiveDocumentEventListeners(
      base::FeatureList::IsEnabled(features::kPassiveDocumentEventListeners));

  WebRuntimeFeatures::EnablePassiveDocumentWheelEventListeners(
      base::FeatureList::IsEnabled(
          features::kPassiveDocumentWheelEventListeners));

  WebRuntimeFeatures::EnableFeatureFromString(
      "FontSrcLocalMatching",
      base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));

  if (command_line.HasSwitch(switches::kDisableBackgroundTimerThrottling))
    WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(false);

  WebRuntimeFeatures::EnableExpensiveBackgroundTimerThrottling(
      base::FeatureList::IsEnabled(
          features::kExpensiveBackgroundTimerThrottling));

  if (base::FeatureList::IsEnabled(features::kHeapCompaction))
    WebRuntimeFeatures::EnableHeapCompaction(true);

  WebRuntimeFeatures::EnableRenderingPipelineThrottling(
      base::FeatureList::IsEnabled(features::kRenderingPipelineThrottling));

  WebRuntimeFeatures::EnableTimerThrottlingForHiddenFrames(
      base::FeatureList::IsEnabled(features::kTimerThrottlingForHiddenFrames));

  if (base::FeatureList::IsEnabled(
          features::kSendBeaconThrowForBlobWithNonSimpleType))
    WebRuntimeFeatures::EnableSendBeaconThrowForBlobWithNonSimpleType(true);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::EnableMediaSession(false);
#endif

  WebRuntimeFeatures::EnablePaymentRequest(
      base::FeatureList::IsEnabled(features::kWebPayments));

  WebRuntimeFeatures::EnablePaymentRequestHasEnrolledInstrument(
      base::FeatureList::IsEnabled(
          features::kPaymentRequestHasEnrolledInstrument));

  if (base::FeatureList::IsEnabled(features::kServiceWorkerPaymentApps))
    WebRuntimeFeatures::EnablePaymentApp(true);

  WebRuntimeFeatures::EnableNetworkService(
      base::FeatureList::IsEnabled(network::features::kNetworkService));

  if (base::FeatureList::IsEnabled(features::kCompositeOpaqueFixedPosition))
    WebRuntimeFeatures::EnableFeatureFromString("CompositeOpaqueFixedPosition",
                                                true);

  if (!base::FeatureList::IsEnabled(features::kCompositeOpaqueScrollers))
    WebRuntimeFeatures::EnableFeatureFromString("CompositeOpaqueScrollers",
                                                false);
  if (base::FeatureList::IsEnabled(features::kCompositorTouchAction))
    WebRuntimeFeatures::EnableCompositorTouchAction(true);

  if (!base::FeatureList::IsEnabled(features::kGenericSensor))
    WebRuntimeFeatures::EnableGenericSensor(false);

  if (base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses))
    WebRuntimeFeatures::EnableGenericSensorExtraClasses(true);

  if (network::features::ShouldEnableOutOfBlinkCors())
    WebRuntimeFeatures::EnableOutOfBlinkCors(true);

  WebRuntimeFeatures::EnableMediaCastOverlayButton(
      base::FeatureList::IsEnabled(media::kMediaCastOverlayButton));

  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources)) {
    WebRuntimeFeatures::EnableFeatureFromString("BlockCredentialedSubresources",
                                                false);
  }

  if (base::FeatureList::IsEnabled(features::kRasterInducingScroll))
    WebRuntimeFeatures::EnableRasterInducingScroll(true);

  WebRuntimeFeatures::EnableFeatureFromString(
      "AllowContentInitiatedDataUrlNavigations",
      base::FeatureList::IsEnabled(
          features::kAllowContentInitiatedDataUrlNavigations));

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc))
    WebRuntimeFeatures::EnableWebNfc(true);
#endif

  WebRuntimeFeatures::EnableWebAuth(
      base::FeatureList::IsEnabled(features::kWebAuth));

  WebRuntimeFeatures::EnableClientPlaceholdersForServerLoFi(
      base::GetFieldTrialParamValue("PreviewsClientLoFi",
                                    "replace_server_placeholders") != "false");

  WebRuntimeFeatures::EnableResourceLoadScheduler(
      base::FeatureList::IsEnabled(features::kResourceLoadScheduler));

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleAll))
    WebRuntimeFeatures::EnableBuiltInModuleAll(true);

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleInfra))
    WebRuntimeFeatures::EnableBuiltInModuleInfra(true);

  if (base::FeatureList::IsEnabled(features::kBuiltInModuleKvStorage))
    WebRuntimeFeatures::EnableBuiltInModuleKvStorage(true);

  if (base::FeatureList::IsEnabled(blink::features::kLayoutNG))
    WebRuntimeFeatures::EnableLayoutNG(true);

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

  if (base::FeatureList::IsEnabled(features::kLazyFrameLoading))
    WebRuntimeFeatures::EnableLazyFrameLoading(true);
  if (base::FeatureList::IsEnabled(features::kLazyFrameVisibleLoadTimeMetrics))
    WebRuntimeFeatures::EnableLazyFrameVisibleLoadTimeMetrics(true);
  if (base::FeatureList::IsEnabled(features::kLazyImageLoading))
    WebRuntimeFeatures::EnableLazyImageLoading(true);
  if (base::FeatureList::IsEnabled(features::kLazyImageVisibleLoadTimeMetrics))
    WebRuntimeFeatures::EnableLazyImageVisibleLoadTimeMetrics(true);

  WebRuntimeFeatures::EnableAutomaticLazyFrameLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading, "automatic-lazy-load-frames-enabled",
          false));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyFrameLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading,
          "restrict-lazy-load-frames-to-data-saver-only", false));

  WebRuntimeFeatures::EnableAutomaticLazyImageLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading, "automatic-lazy-load-images-enabled",
          false));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyImageLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading,
          "restrict-lazy-load-images-to-data-saver-only", false));

  WebRuntimeFeatures::EnablePictureInPicture(
      base::FeatureList::IsEnabled(media::kPictureInPicture));

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnablePictureInPictureAPI(
      base::FeatureList::IsEnabled(media::kPictureInPictureAPI));
#endif

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

#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    // Display Cutout is limited to Android P+.
    WebRuntimeFeatures::EnableDisplayCutoutAPI(true);
  }
#endif

  if (command_line.HasSwitch(switches::kEnableAccessibilityObjectModel))
    WebRuntimeFeatures::EnableAccessibilityObjectModel(true);

  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI)) {
    WebRuntimeFeatures::EnableFeatureFromString("NativeFileSystem", true);
    if (base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI))
      WebRuntimeFeatures::EnableFeatureFromString("FileHandling", true);
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kForbidSyncXHRInPageDismissal)) {
    WebRuntimeFeatures::EnableForbidSyncXHRInPageDismissal(true);
  }

  WebRuntimeFeatures::EnableAutoplayIgnoresWebAudio(
      base::FeatureList::IsEnabled(media::kAutoplayIgnoreWebAudio));

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnableMediaControlsExpandGesture(
      base::FeatureList::IsEnabled(media::kMediaControlsExpandGesture));
#endif

  WebRuntimeFeatures::EnablePortals(
      base::FeatureList::IsEnabled(blink::features::kPortals));

  WebRuntimeFeatures::EnableImplicitRootScroller(
      base::FeatureList::IsEnabled(blink::features::kImplicitRootScroller));

  WebRuntimeFeatures::EnableTextFragmentAnchor(
      base::FeatureList::IsEnabled(blink::features::kTextFragmentAnchor));

  if (!base::FeatureList::IsEnabled(features::kBackgroundFetch))
    WebRuntimeFeatures::EnableBackgroundFetch(false);

  WebRuntimeFeatures::EnableUpdateHoverFromLayoutChangeAtBeginFrame(
      base::FeatureList::IsEnabled(
          features::kUpdateHoverFromLayoutChangeAtBeginFrame));

  // TODO(yashard): Remove |enable_experimental_web_platform_features| flag
  // since the feature should have been enabled when it is set to experimental
  WebRuntimeFeatures::EnableJankTrackingSweepLine(
      base::FeatureList::IsEnabled(blink::features::kJankTrackingSweepLine) ||
      enable_experimental_web_platform_features);

  WebRuntimeFeatures::EnableFirstContentfulPaintPlusPlus(
      base::FeatureList::IsEnabled(
          blink::features::kFirstContentfulPaintPlusPlus));

  WebRuntimeFeatures::EnableUpdateHoverFromScrollAtBeginFrame(
      base::FeatureList::IsEnabled(
          features::kUpdateHoverFromScrollAtBeginFrame));

  WebRuntimeFeatures::EnableGetDisplayMedia(
      base::FeatureList::IsEnabled(blink::features::kRTCGetDisplayMedia));

  WebRuntimeFeatures::EnableMimeHandlerViewInCrossProcessFrame(
      base::FeatureList::IsEnabled(
          features::kMimeHandlerViewInCrossProcessFrame));

  WebRuntimeFeatures::EnableFallbackCursorMode(
      base::FeatureList::IsEnabled(features::kFallbackCursorMode));

  if (base::FeatureList::IsEnabled(features::kUserAgentClientHint))
    WebRuntimeFeatures::EnableFeatureFromString("UserAgentClientHint", true);

  WebRuntimeFeatures::EnableSignedExchangeSubresourcePrefetch(
      base::FeatureList::IsEnabled(
          features::kSignedExchangeSubresourcePrefetch));

  if (!base::FeatureList::IsEnabled(features::kIdleDetection))
    WebRuntimeFeatures::EnableIdleDetection(false);

  WebRuntimeFeatures::EnableSkipTouchEventFilter(
      base::FeatureList::IsEnabled(features::kSkipTouchEventFilter));

  WebRuntimeFeatures::EnableStaleWhileRevalidate(
      base::FeatureList::IsEnabled(features::kStaleWhileRevalidate));

  if (!base::FeatureList::IsEnabled(features::kSmsReceiver))
    WebRuntimeFeatures::EnableSmsReceiver(false);

  WebRuntimeFeatures::EnableDisplayLocking(
      base::FeatureList::IsEnabled(blink::features::kDisplayLocking));

  WebRuntimeFeatures::EnableFormControlsRefresh(
      features::IsFormControlsRefreshEnabled());

  if (base::FeatureList::IsEnabled(
      blink::features::kAudioWorkletRealtimeThread)) {
    WebRuntimeFeatures::EnableFeatureFromString(
        "AudioWorkletRealtimeThread", true);
  }

  if (!base::FeatureList::IsEnabled(
          features::kPauseExecutionContextOnBackgroundFreeze)) {
    WebRuntimeFeatures::EnablePauseExecutionContextOnBackgroundFreeze(false);
  }

  WebRuntimeFeatures::EnableConsolidatedMovementXY(
      base::FeatureList::IsEnabled(features::kConsolidatedMovementXY));
}

}  // namespace

namespace content {

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  // Set experimental features
  bool enable_experimental_web_platform_features =
      command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  if (enable_experimental_web_platform_features)
    WebRuntimeFeatures::EnableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform();

  // Set origin trial features
  if (command_line.HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }

  // TODO(yashard): Remove |enable_experimental_web_platform_features|
  // flag since no individual feature should need it
  SetIndividualRuntimeFeatures(command_line,
                               enable_experimental_web_platform_features);

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
