// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "chrome/android/chrome_jni_headers/ChromeFeatureList_jni.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/download/public/common/download_features.h"
#include "components/feed/feed_feature_list.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/previews/core/previews_features.h"
#include "components/safe_browsing/features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/unified_consent/feature.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/ui_base_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* kFeaturesExposedToJava[] = {
    &autofill::features::kAutofillKeyboardAccessory,
    &autofill::features::kAutofillManualFallbackAndroid,
    &autofill::features::kAutofillRefreshStyleAndroid,
    &autofill::features::kAutofillEnableCompanyName,
    &autofill_assistant::features::kAutofillAssistant,
    &autofill_assistant::features::kAutofillAssistantDirectActions,
    &autofill::features::kTouchToFillAndroid,
    &download::features::kDownloadAutoResumptionNative,
    &download::features::kUseDownloadOfflineContentProvider,
    &features::kAppNotificationStatusMessaging,
    &features::kCaptionSettings,
    &features::kClearOldBrowsingData,
    &features::kDownloadsLocationChange,
    &features::kGenericSensorExtraClasses,
    &features::kInstallableAmbientBadgeInfoBar,
    &features::kOverscrollHistoryNavigation,
    &features::kPermissionDelegation,
    &features::kPredictivePrefetchingAllowedOnAllConnectionTypes,
    &features::kPrioritizeBootstrapTasks,
    &features::kServiceWorkerPaymentApps,
    &features::kSensorContentSetting,
    &features::kShowTrustedPublisherURL,
    &features::kUserActivationV2,
    &features::kWebAuth,
    &features::kWebPayments,
    &feed::kInterestFeedContentSuggestions,
    &invalidation::switches::kFCMInvalidations,
    &kAdjustWebApkInstallationSpace,
    &kAllowNewIncognitoTabIntents,
    &kAllowRemoteContextForNotifications,
    &kAndroidNightMode,
    &kAndroidNightModeCCT,
    &kAndroidNightModeForQ,
    &kAndroidPayIntegrationV1,
    &kAndroidPayIntegrationV2,
    &kAndroidPaymentApps,
    &kAndroidSearchEngineChoiceNotification,
    &kAndroidSiteSettingsUIRefresh,
    &kBackgroundTaskSchedulerForBackgroundSync,
    &kCastDeviceFilter,
    &kCCTBackgroundTab,
    &kCCTExternalLinkHandling,
    &kCCTModule,
    &kCCTModuleCache,
    &kCCTModuleCustomHeader,
    &kCCTModuleCustomRequestHeader,
    &kCCTModuleDexLoading,
    &kCCTModulePostMessage,
    &kCCTModuleUseIntentExtras,
    &kCCTPostMessageAPI,
    &kCCTRedirectPreconnect,
    &kCCTReportParallelRequestStatus,
    &kCCTResourcePrefetch,
    &kCCTTargetTranslateLanguage,
    &kChromeDuetFeature,
    &kChromeDuetAdaptive,
    &kDontAutoHideBrowserControls,
    &kChromeDuetLabeled,
    &kChromeSmartSelection,
    &kCommandLineOnNonRooted,
    &kContactsPickerSelectAll,
    &kContentSuggestionsScrollToLoad,
    &kContextualSearchDefinitions,
    &kContextualSearchLongpressResolve,
    &kContextualSearchMlTapSuppression,
    &kContextualSearchSecondTap,
    &kContextualSearchSimplifiedServer,
    &kContextualSearchTapDisableOverride,
    &kContextualSearchTranslationModel,
    &kContextualSearchUnityIntegration,
    &kDirectActions,
    &kDownloadLocationShowImageInGallery,
    &kDownloadFileProvider,
    &kDownloadHomeV2,
    &kDownloadHomeShowStorageInfo,
    &kDownloadProgressInfoBar,
    &kDownloadRename,
    &kDownloadTabManagementModule,
    &kDrawVerticallyEdgeToEdge,
    &kEphemeralTab,
    &kExploreSites,
    &kHandleMediaIntents,
    &kHomePageButtonForceEnabled,
    &kHomepageTile,
    &kHorizontalTabSwitcherAndroid,
    &kIdentityDisc,
    &kImmersiveUiMode,
    &kImprovedA2HS,
    &kInlineUpdateFlow,
    &kIntentBlockExternalFormRedirectsNoGesture,
    &kJellyBeanSupported,
    &kNewPhotoPicker,
    &kNotificationSuspender,
    &kNoCreditCardAbort,
    &kNTPButton,
    &kNTPLaunchAfterInactivity,
    &kOmniboxSpareRenderer,
    &kOverlayNewLayout,
    &kPayWithGoogleV1,
    &kPhotoPickerVideoSupport,
    &kProgressBarThrottleFeature,
    &kReachedCodeProfiler,
    &kReaderModeInCCT,
    &kReorderBookmarks,
    &kRevampedContextMenu,
    &kSearchReadyOmniboxFeature,
    &kSearchEnginePromoExistingDevice,
    &kSearchEnginePromoNewDevice,
    &kServiceManagerForBackgroundPrefetch,
    &kServiceManagerForDownload,
    &kShoppingAssist,
    &kSpannableInlineAutocomplete,
    &kSpecialLocaleWrapper,
    &kSpecialUserDecision,
    &kSwapPixelFormatToFixConvertFromTranslucent,
    &kTabEngagementReportingAndroid,
    &kTabGroupsAndroid,
    &kTabGroupsUiImprovementsAndroid,
    &kTabGridLayoutAndroid,
    &kTabReparenting,
    &kTabSwitcherOnReturn,
    &kTabToGTSAnimation,
    &kTrustedWebActivity,
    &kTrustedWebActivityPostMessage,
    &kTrustedWebActivityNotificationDelegationEnrolment,
    &kTwoPanesStartSurfaceAndroid,
    &kUmaBackgroundSessions,
    &kUsageStatsFeature,
    &kVideoPersistence,
    &kVrBrowsingFeedback,
    &kOfferWipeDataOnSignout,
    &network::features::kNetworkService,
    &payments::features::kAlwaysAllowJustInTimePaymentApp,
    &payments::features::kReturnGooglePayInBasicCard,
    &payments::features::kWebPaymentsExperimentalFeatures,
    &payments::features::kWebPaymentsMethodSectionOrderV2,
    &payments::features::kWebPaymentsModifiers,
    &payments::features::kWebPaymentsSingleAppUiSkip,
    &payments::features::kWebPaymentsRedactShippingAddress,
    &language::kExplicitLanguageAsk,
    &ntp_snippets::kArticleSuggestionsFeature,
    &offline_pages::kOfflineIndicatorFeature,
    &offline_pages::kOfflineIndicatorAlwaysHttpProbeFeature,
    &offline_pages::kOfflinePagesCTFeature,    // See crbug.com/620421.
    &offline_pages::kOfflinePagesCTV2Feature,  // See crbug.com/734753.
    &offline_pages::kOfflinePagesDescriptiveFailStatusFeature,
    &offline_pages::kOfflinePagesDescriptivePendingStatusFeature,
    &offline_pages::kOfflinePagesLivePageSharingFeature,
    &offline_pages::kPrefetchingOfflinePagesFeature,
    &omnibox::kHideSteadyStateUrlScheme,
    &omnibox::kHideSteadyStateUrlTrivialSubdomains,
    &omnibox::kOmniboxRichEntitySuggestions,
    &omnibox::kQueryInOmnibox,
    &omnibox::kUIExperimentShowSuggestionFavicons,
    &omnibox::kOmniboxSearchEngineLogo,
    &password_manager::features::kGooglePasswordManager,
    &password_manager::features::kPasswordEditingAndroid,
    &safe_browsing::kCaptureSafetyNetId,
    &signin::kMiceFeature,
    &switches::kSyncSendTabToSelf,
    &translate::kTranslateMobileManualTrigger,
    &unified_consent::kUnifiedConsent,
    &subresource_filter::kSafeBrowsingSubresourceFilter,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (size_t i = 0; i < base::size(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return kFeaturesExposedToJava[i];
  }
  NOTREACHED() << "Queried feature cannot be found in ChromeFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

// Alphabetical:
const base::Feature kAdjustWebApkInstallationSpace = {
    "AdjustWebApkInstallationSpace", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAndroidNightMode{"AndroidNightMode",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAndroidNightModeCCT{"AndroidNightModeCCT",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidNightModeForQ{"AndroidNightModeForQ",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kAndroidPayIntegrationV1{"AndroidPayIntegrationV1",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAllowNewIncognitoTabIntents{
    "AllowNewIncognitoTabIntents", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAllowRemoteContextForNotifications{
    "AllowRemoteContextForNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kAndroidPayIntegrationV2{"AndroidPayIntegrationV2",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kAndroidPaymentApps{"AndroidPaymentApps",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidSearchEngineChoiceNotification{
    "AndroidSearchEngineChoiceNotification", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidSiteSettingsUIRefresh{
    "AndroidSiteSettingsUIRefresh", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidWebContentsDarkMode{
    "AndroidWebContentsDarkMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBackgroundTaskComponentUpdate{
    "BackgroundTaskComponentUpdate", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBackgroundTaskSchedulerForBackgroundSync{
    "BackgroundTaskSchedulerForBackgroundSync",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Used in downstream code.
const base::Feature kCastDeviceFilter{"CastDeviceFilter",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTBackgroundTab{"CCTBackgroundTab",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTExternalLinkHandling{"CCTExternalLinkHandling",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTModule{"CCTModule", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModuleCache{"CCTModuleCache",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModuleCustomHeader{"CCTModuleCustomHeader",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModuleCustomRequestHeader{
    "CCTModuleCustomRequestHeader", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModuleDexLoading{"CCTModuleDexLoading",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModulePostMessage{"CCTModulePostMessage",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTModuleUseIntentExtras{
    "CCTModuleUseIntentExtras", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTPostMessageAPI{"CCTPostMessageAPI",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTRedirectPreconnect{"CCTRedirectPreconnect",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTReportParallelRequestStatus{
    "CCTReportParallelRequestStatus", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTResourcePrefetch{"CCTResourcePrefetch",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTTargetTranslateLanguage{
    "CCTTargetTranslateLanguage", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kChromeDuetFeature{"ChromeDuet",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeDuetAdaptive{"ChromeDuetAdaptive",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDontAutoHideBrowserControls{
    "DontAutoHideBrowserControls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeDuetLabeled{"ChromeDuetLabeled",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeSmartSelection{"ChromeSmartSelection",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCommandLineOnNonRooted{"CommandLineOnNonRooted",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContactsPickerSelectAll{"ContactsPickerSelectAll",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsScrollToLoad{
    "ContentSuggestionsScrollToLoad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchDefinitions{
    "ContextualSearchDefinitions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchLongpressResolve{
    "ContextualSearchLongpressResolve", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchMlTapSuppression{
    "ContextualSearchMlTapSuppression", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchSecondTap{
    "ContextualSearchSecondTap", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchSimplifiedServer{
    "ContextualSearchSimplifiedServer", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchTapDisableOverride{
    "ContextualSearchTapDisableOverride", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchTranslationModel{
    "ContextualSearchTranslationModel", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchUnityIntegration{
    "ContextualSearchUnityIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDirectActions{"DirectActions",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDrawVerticallyEdgeToEdge{
    "DrawVerticallyEdgeToEdge", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadAutoResumptionThrottling{
    "DownloadAutoResumptionThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadLocationShowImageInGallery{
    "DownloadLocationShowImageInGallery", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadProgressInfoBar{"DownloadProgressInfoBar",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadFileProvider{"DownloadFileProvider",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadHomeV2{"DownloadHomeV2",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadHomeShowStorageInfo{
    "DownloadHomeShowStorageInfo", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadRename{"DownloadRename",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadTabManagementModule{
    "DownloadTabManagementModule", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEphemeralTab{"EphemeralTab",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExploreSites{"ExploreSites",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForegroundNotificationManager{
    "ForegroundNotificationManager", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHandleMediaIntents{"HandleMediaIntents",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHomePageButtonForceEnabled{
    "HomePageButtonForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHomepageTile{"HomepageTile",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHorizontalTabSwitcherAndroid{
    "HorizontalTabSwitcherAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIdentityDisc{"IdentityDisc",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Makes "Add to Home screen" in the app menu generate an APK for the shortcut
// URL which opens Chrome in fullscreen.
// This feature is kept around so that we have a kill-switch in case of server
// issues.
const base::Feature kImprovedA2HS{"ImprovedA2HS",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kImmersiveUiMode{"ImmersiveUiMode",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInlineUpdateFlow{"InlineUpdateFlow",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIntentBlockExternalFormRedirectsNoGesture{
    "IntentBlockExternalFormRedirectsNoGesture",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kJellyBeanSupported{"JellyBeanSupported",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoExistingDevice{
    "SearchEnginePromo.ExistingDevice", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoNewDevice{
    "SearchEnginePromo.NewDevice", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNewPhotoPicker{"NewPhotoPicker",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(knollr): This is a temporary kill switch, it can be removed once we feel
// okay about leaving it on.
const base::Feature kNotificationSuspender{"NotificationSuspender",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNoCreditCardAbort{"NoCreditCardAbort",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNTPButton{"NTPButton", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNTPLaunchAfterInactivity{
    "NTPLaunchAfterInactivity", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxSpareRenderer{"OmniboxSpareRenderer",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOverlayNewLayout{"OverlayNewLayout",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kPayWithGoogleV1{"PayWithGoogleV1",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(finnur): Before enabling by default, the issue of where decoding should
// take place needs to be resolved.
const base::Feature kPhotoPickerVideoSupport{"PhotoPickerVideoSupport",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kProgressBarThrottleFeature{
    "ProgressBarThrottle", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReachedCodeProfiler{"ReachedCodeProfiler",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReaderModeInCCT{"ReaderModeInCCT",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kReorderBookmarks{"ReorderBookmarks",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRevampedContextMenu{"RevampedContextMenu",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchReadyOmniboxFeature{
    "SearchReadyOmnibox", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceManagerForBackgroundPrefetch{
    "ServiceManagerForBackgroundPrefetch", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kServiceManagerForDownload{
    "ServiceManagerForDownload", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShoppingAssist{"ShoppingAssist",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSpannableInlineAutocomplete{
    "SpannableInlineAutocomplete", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSpecialLocaleWrapper{"SpecialLocaleWrapper",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSpecialUserDecision{"SpecialUserDecision",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSwapPixelFormatToFixConvertFromTranslucent{
    "SwapPixelFormatToFixConvertFromTranslucent",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabEngagementReportingAndroid{
    "TabEngagementReportingAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGroupsAndroid{"TabGroupsAndroid",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGroupsUiImprovementsAndroid{
    "TabGroupsUiImprovementsAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGridLayoutAndroid{"TabGridLayoutAndroid",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabReparenting{"TabReparenting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabSwitcherOnReturn{"TabSwitcherOnReturn",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabToGTSAnimation{"TabToGTSAnimation",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivity{"TrustedWebActivity",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityPostMessage{
    "TrustedWebActivityPostMessage", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityNotificationDelegationEnrolment{
    "TrustedWebActivityNotificationDelegationAutoEnrolment",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTwoPanesStartSurfaceAndroid{
    "TwoPanesStartSurfaceAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, keep logging and reporting UMA while chrome is backgrounded.
const base::Feature kUmaBackgroundSessions{"UMABackgroundSessions",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUsageStatsFeature{"UsageStats",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserMediaScreenCapturing{
    "UserMediaScreenCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVideoPersistence{"VideoPersistence",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kVrBrowsingFeedback{"VrBrowsingFeedback",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfferWipeDataOnSignout{"OfferWipeDataOnSignout",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

static jboolean JNI_ChromeFeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

static jboolean JNI_ChromeFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

static ScopedJavaLocalRef<jstring>
JNI_ChromeFeatureList_GetFieldTrialParamByFeature(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  const std::string& param_value =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

static jint JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jint jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static jdouble JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsDouble(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jdouble jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsDouble(*feature, param_name,
                                                   jdefault_value);
}

static jboolean JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

}  // namespace android
}  // namespace chrome
