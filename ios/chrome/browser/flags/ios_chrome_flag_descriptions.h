// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to control the new app launcher.
extern const char kAppLauncherRefreshName[];
extern const char kAppLauncherRefreshDescription[];

// Title and description for the flag to control the autofill query cache.
extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control GPay branding in credit card
// downstream keyboard accessory.
extern const char kAutofillDownstreamUseGooglePayBrandingOniOSName[];
extern const char kAutofillDownstreamUseGooglePayBrandingOniOSDescription[];

// Title and description for the flag to control deprecating company name.
extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to control offering to save unmasked
// server cards locally as FULL_SERVER_CARDs upon success of credit card unmask.
extern const char kAutofillNoLocalSaveOnUnmaskSuccessName[];
extern const char kAutofillNoLocalSaveOnUnmaskSuccessDescription[];

// Title and description for the flag to control saving FULL_SERVER_CARDS upon
// success of credit card upload.
extern const char kAutofillNoLocalSaveOnUploadSuccessName[];
extern const char kAutofillNoLocalSaveOnUploadSuccessDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to control if prefilled value filter
// profiles.
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsName[];
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[];

// Title and description for the flag to restrict extraction of formless forms
// to checkout flows.
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

// Title and description for the flag to enable rich autofill queries on
// Canary/Dev.
extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Title and description for the flag to make browser container keep the
// content view hierarchy directly.
extern const char kBrowserContainerKeepsContentViewName[];
extern const char kBrowserContainerKeepsContentViewDescription[];

// Title and description for the flag to control redirection to the task
// scheduler.
extern const char kBrowserTaskScheduler[];
extern const char kBrowserTaskSchedulerDescription[];

// Title and description for the flag to enable automatically switching to the
// regular tabs after closing the last incognito tab.
extern const char kClosingLastIncognitoTabName[];
extern const char kClosingLastIncognitoTabDescription[];

// Title and description for the flag that controls whether Collections are
// presented using the new iOS13 Card style or the custom legacy one.
extern const char kCollectionsCardPresentationStyleName[];
extern const char kCollectionsCardPresentationStyleDescription[];

// Title and description for the flag to enable Contextual Search.
extern const char kContextualSearch[];
extern const char kContextualSearchDescription[];

// Title and description for the flag to diffentiate between copied
// urls, strings, and images.
extern const char kCopiedContentBehaviorName[];
extern const char kCopiedContentBehaviorDescription[];

#if defined(DCHECK_IS_CONFIGURABLE)
// Title and description for the flag to enable configurable DCHECKs.
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to enable drag and drop.
extern const char kDragAndDropName[];
extern const char kDragAndDropDescription[];

// Title and description for the flag to control the autocomplete retention
// policy.
extern const char kEnableAutocompleteDataRetentionPolicyName[];
extern const char kEnableAutocompleteDataRetentionPolicyDescription[];

// Title and description for the flag to control the updated prompt explanation
// when offering credit card upload.
extern const char kEnableAutofillCreditCardUploadUpdatePromptExplanationName[];
extern const char
    kEnableAutofillCreditCardUploadUpdatePromptExplanationDescription[];

// Title and description for the flag to control if cards from unsupported
// networks should be prevented form being uploaded.
extern const char kEnableAutofillDoNotUploadSaveUnsupportedCardsName[];
extern const char kEnableAutofillDoNotUploadSaveUnsupportedCardsDescription[];

// Title and description for the flag to control if no thanks button should be
// shown when saving a card.
extern const char kEnableAutofillSaveCardShowNoThanksName[];
extern const char kEnableAutofillSaveCardShowNoThanksDescription[];

// Title and description for the flag to control if credit card save should
// utilize the Autofill StrikeDatabase when determining whether save
// should be offered.
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[];
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[];

// Title and description for the flag to control the credit card import from
// dynamic forms.
extern const char kEnableAutofillImportDynamicFormsName[];
extern const char kEnableAutofillImportDynamicFormsDescription[];

// Title and description for the flag to control the credit card import from
// accordion forms.
extern const char kEnableAutofillImportNonFocusableCreditCardFormsName[];
extern const char kEnableAutofillImportNonFocusableCreditCardFormsDescription[];

// Title and description for the flag to enable the clipboard provider to
// suggest searchihng for copied imagse
extern const char kEnableClipboardProviderImageSuggestionsName[];
extern const char kEnableClipboardProviderImageSuggestionsDescription[];

// Title and description for the flag to enable the clipboard provider to
// suggest copied text
extern const char kEnableClipboardProviderTextSuggestionsName[];
extern const char kEnableClipboardProviderTextSuggestionsDescription[];

extern const char kEnableSyncUSSBookmarksName[];
extern const char kEnableSyncUSSBookmarksDescription[];

extern const char kEnableSyncUSSPasswordsName[];
extern const char kEnableSyncUSSPasswordsDescription[];

extern const char kEnableSyncUSSNigoriName[];
extern const char kEnableSyncUSSNigoriDescription[];

// Title and description for the flag to enable invaliations delivery via FCM.
extern const char kFCMInvalidationsName[];
extern const char kFCMInvalidationsDescription[];

// Title and description for the flag to enable fill passwords on account select
// on HTTP origins.
extern const char kFillOnAccountSelectHttpName[];
extern const char kFillOnAccountSelectHttpDescription[];

// Title and description for the flag to search in iFrames in Find In Page.
extern const char kFindInPageiFrameName[];
extern const char kFindInPageiFrameDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenViewportAdjustmentExperimentName[];
extern const char kFullscreenViewportAdjustmentExperimentDescription[];

// Title and description for the flag to enable History batch filtering.
extern const char kHistoryBatchUpdatesFilterName[];
extern const char kHistoryBatchUpdatesFilterDescription[];

// Title and description for the flag to display current user identity on
// New Tab Page.
extern const char kIdentityDiscName[];
extern const char kIdentityDiscDescription[];

// Title and description for the flag to ignore viewport scale limits.
extern const char kIgnoresViewportScaleLimitsName[];
extern const char kIgnoresViewportScaleLimitsDescription[];

// Title and description for the flag to enable the new UI Reboot on Infobars.
extern const char kInfobarUIRebootName[];
extern const char kInfobarUIRebootDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable the language settings page.
extern const char kLanguageSettingsName[];
extern const char kLanguageSettingsDescription[];

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];

// Title and description for the flag to enable new Clear Browsing Data UI.
extern const char kNewClearBrowsingDataUIName[];
extern const char kNewClearBrowsingDataUIDescription[];

// Title and description for the flag to display new omnibox popup.
extern const char kNewOmniboxPopupLayoutName[];
extern const char kNewOmniboxPopupLayoutDescription[];

// Title and description for the flag to enable non-modal JavaScript dialogs.
extern const char kNonModalDialogsName[];
extern const char kNonModalDialogsDescription[];

// Title and description for the flag to display offline pages directly in the
// web view.
extern const char kOfflineVersionWithoutNativeContentName[];
extern const char kOfflineVersionWithoutNativeContentDescription[];

// Title and description for the flag to show most visited sites and collection
// shortcuts in the omnibox popup instead of ZeroSuggest.
extern const char kOmniboxPopupShortcutIconsInZeroStateName[];
extern const char kOmniboxPopupShortcutIconsInZeroStateDescription[];

// Title and description for the flag to enable the "switch to this tab" option
// in the omnibox suggestion. It doesn't add new suggestions.
extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

// Title and description for the flag to enable elision of the URL path, query,
// and ref in omnibox URL suggestions.
extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

// Title and description for the flag to change the max number of autocomplete
// matches in the omnibox popup.
extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

// Title and description for the flag to show default search engine favicon in
// the omnibox
extern const char kOmniboxUseDefaultSearchEngineFaviconName[];
extern const char kOmniboxUseDefaultSearchEngineFaviconDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions.
extern const char kOmniboxOnDeviceHeadSuggestionsName[];
extern const char kOmniboxOnDeviceHeadSuggestionsDescription[];

// Title and description for the flag to enable optional thumbnail for NTP
// articles according to European copyright directive(EUCD).
extern const char kOptionalArticleThumbnailName[];
extern const char kOptionalArticleThumbnailDescription[];

// Title and description for the flag to enable password generation.
extern const char kPasswordGenerationName[];
extern const char kPasswordGenerationDescription[];

// Title and description for the flag to toggle the flag of the search button.
extern const char kSearchIconToggleName[];
extern const char kSearchIconToggleDescription[];

// Title and description for the flag to enable the send tab to self receiving
// feature.
extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

// Title and description for the flag to enable the tab to be broadcasted to all
// of the users devices.
extern const char kSendTabToSelfBroadcastName[];
extern const char kSendTabToSelfBroadcastDescription[];

// Title and description for the flag to enable the send tab to self sending UI.
extern const char kSendTabToSelfShowSendingUIName[];
extern const char kSendTabToSelfShowSendingUIDescription[];

// Title and description for the flag to send UMA data over any network.
extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

// Title and description for the flag to add a new credit card.
extern const char kSettingsAddPaymentMethodName[];
extern const char kSettingsAddPaymentMethodDescription[];

// Title and description for the flag to toggle the flag for the settings UI
// Refresh.
extern const char kSettingsRefreshName[];
extern const char kSettingsRefreshDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to enable WKBackForwardList based
// navigation manager.
extern const char kSlimNavigationManagerName[];
extern const char kSlimNavigationManagerDescription[];

// Title and description for the flag to use |-drawViewHierarchy:| for taking
// snapshots.
extern const char kSnapshotDrawViewName[];
extern const char kSnapshotDrawViewDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag to control if Chrome Sync (in standalone
// transport mode) supports non-primary accounts.
extern const char kSyncSupportSecondaryAccountName[];
extern const char kSyncSupportSecondaryAccountDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable the new tab button in the
// toolbar.
extern const char kToolbarNewTabButtonName[];
extern const char kToolbarNewTabButtonDescription[];

// Title and description for the flag to control manual translate trigger.
extern const char kTranslateManualTriggerName[];
extern const char kTranslateManualTriggerDescription[];

// Title and description for the flag to enable the unified consent.
extern const char kUnifiedConsentName[];
extern const char kUnifiedConsentDescription[];

// Title and description for the flag to enable the ddljson Doodle API.
extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

// Title and description for the flag to enable Gaia Auth Mutlilogin endpoint
// for identity consistency.
extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

// Title and description for the flag to switch from WKWebView to NSURLSession
// to make sign-in requests to Gaia with attached cookies.
extern const char kUseNSURLSessionForGaiaSigninRequestsName[];
extern const char kUseNSURLSessionForGaiaSigninRequestsDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to use the Clear browsing data API from
// web.
extern const char kWebClearBrowsingDataName[];
extern const char kWebClearBrowsingDataDescription[];

// Title and description for the flag to enable text accessibility in webpages.
extern const char kWebPageTextAccessibilityName[];
extern const char kWebPageTextAccessibilityDescription[];

// Title and description for the flag to enable WKHTTPSystemCookieStore usage
// for main context URL requests.
extern const char kWKHTTPSystemCookieStoreName[];
extern const char kWKHTTPSystemCookieStoreDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
