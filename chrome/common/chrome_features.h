// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAddToHomescreenMessaging;
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kApkWebAppInstalls;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppleScriptExecuteJavaScriptMenuItem;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShow10_9ObsoleteInfobar;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kViewsTaskManager;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppNotificationStatusMessaging;
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceAsh;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceServer;
#endif  // !defined(OS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kAsyncDns;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutoFetchOnNetErrorPage;
#endif

#if defined(OS_WIN) || defined(OS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // defined(OS_WIN) || defined(OS_LINUX)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockPromptsIfDismissedOften;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockPromptsIfIgnoredOften;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBlockRepeatedNotificationPermissionPrompts;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBrowserHangFixesExperiment;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBundledConnectionHelpFeature;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCaptionSettings;

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCertDualVerificationTrialFeature;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChangePictureVideoMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDMServerOAuthForChildUser;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClearOldBrowsingData;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClickToOpenPDFPlaceholder;

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kImmersiveFullscreen;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrostini;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdditionalEnterpriseReporting;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdvancedAccessControls;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAppUninstallGui;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAnsibleInfrastructure;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCupsPrintersUiOverhaul;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kPluginVm;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUsageTimeLimitPolicy;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWilcoDtc;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUploadZippedSystemLogs;
#endif

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTerminalSystemApp;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopCaptureTabSharingInfobar;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsWithoutExtensions;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsLocalUpdating;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsUnifiedInstall;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsUnifiedLaunch;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsUSS;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsOmniboxInstall;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDisallowUnsafeHttpDownloads;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kDisallowUnsafeHttpDownloadsParamName[];

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDownloadsLocationChange;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDriveFcmInvalidations;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPolicyFcmInvalidations;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInGuestSession;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInIncognito;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableIncognitoWindowCounter;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnterpriseReportingInBrowser;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEventBasedStatusReporting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExternalExtensionDefaultButtonControl;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFocusMode;

// Android expects this string from Java code, so it is always needed.
// TODO(crbug.com/731802): Use #if BUILDFLAG(ENABLE_VR_BROWSING) instead.
#if BUILDFLAG(ENABLE_VR) || defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kVrBrowsing;
#endif
#if BUILDFLAG(ENABLE_VR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalFeatures;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalRendering;

#if BUILDFLAG(ENABLE_OCULUS_VR)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOculusVR;
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOpenVR;
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWindowsMixedReality;
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_OPENXR)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOpenXR;
#endif  // ENABLE_OPENXR

#endif  // ENABLE_VR

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGdiTextPrinting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGeoLanguage;

#if !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kGoogleBrandedContextMenu;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystem;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktop;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHTTPAuthCommittedInterstitials;

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncompatibleApplicationsWarning;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInSessionPasswordChange;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInstallableAmbientBadgeInfoBar;
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIntentPicker;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kKernelnextVMs;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kKidsManagementUrlClassification;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLookalikeUrlNavigationSuggestionsUI;

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacFullSizeContentView;
#endif

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacMaterialDesignDownloadShelf;
#endif

#if defined(OS_MACOSX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacSystemMediaPermissionsInfoUi;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAcknowledgeNtpOverrideOnDeactivate;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kManagedGuestSessionNotification;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kModalPermissionPrompts;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOnConnectNative;
#endif

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNativeNotifications;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNewNetErrorPageUI;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNewTabLoadingAnimation;
#endif

#if defined(OS_POSIX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNtlmV2Enabled;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOomIntervention;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseNewAcceptLanguageHeader;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kParentAccessCode;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kParentAccessCodeForTimeChange;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPermissionDelegation;

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDisablePostScriptPrinting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrerenderFallbackToPreconnect;

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFlashDeprecationWarning;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCloudPrinterHandler;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNewPrintPreviewLayout;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPushMessagingBackgroundMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRemoveSupervisedUsersOnStartup;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLoadBrokenImagesFromContextMenu;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSafetyTipUI;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSecurityKeyAttestationPrompt;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShowTrustedPublisherURL;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSitePerProcess;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSiteIsolationForPasswordSites;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSitePerProcessOnlyForHighMemoryClients;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kSitePerProcessOnlyForHighMemoryClientsParamName[];

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kStreamlinedUsbPrinterSetup;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNativeSmb;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSoundContentSetting;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSysInternals;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSystemWebApps;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppManagement;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTabMetricsLogging;
#endif

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kThirdPartyModulesBlocking;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kTreatUnsafeDownloadsAsActive;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kTreatUnsafeDownloadsAsActiveParamName[];

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUsageTimeStateNotifier;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseDisplayWideColorGamut;

COMPONENT_EXPORT(CHROME_FEATURES)
bool UseDisplayWideColorGamut();
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseFtlSignalingForCrdHostDelegate;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAdaptiveScreenBrightnessLogging;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUserActivityEventLogging;

#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUseSameCacheForMedia;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kArcCupsApi;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrosVmCupsProxy;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kQuickUnlockPin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuickUnlockPinSignin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuickUnlockFingerprint;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kBulkPrinters;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrosCompUpdates;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTPMFirmwareUpdate;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrOSEnableUSMUserService;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMachineLearningService;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbguard;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbbouncer;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSchedulerConfiguration;
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLog;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLogGzipped;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWebUIDarkMode;

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWin10AcceleratedDefaultBrowserFlow;
#endif  // defined(OS_WIN)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWriteBasicSystemProfileToPersistentHistogramsFile;

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
