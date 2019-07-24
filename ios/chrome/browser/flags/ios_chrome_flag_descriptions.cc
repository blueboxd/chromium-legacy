// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAppLauncherRefreshName[] = "Enable the new AppLauncher logic";
const char kAppLauncherRefreshDescription[] =
    "AppLauncher will always prompt if there is no direct link navigation, "
    "also Apps will launch asynchronously and there will be no logic that"
    "depends on the success or the failure of launching an app.";

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDownstreamUseGooglePayBrandingOniOSName[] =
    "Enable Google Pay branding when offering credit card downstream";
const char kAutofillDownstreamUseGooglePayBrandingOniOSDescription[] =
    "When enabled, shows the Google Pay logo animation when showing payments"
    "credit card suggestions in downstream keyboard accessory";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[] =
    "Autofill Enforce Min Required Fields For Heuristics";
const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before allowing heuristic field-type prediction to occur.";

const char kAutofillEnforceMinRequiredFieldsForQueryName[] =
    "Autofill Enforce Min Required Fields For Query";
const char kAutofillEnforceMinRequiredFieldsForQueryDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before querying the autofill server for field-type predictions.";

const char kAutofillEnforceMinRequiredFieldsForUploadName[] =
    "Autofill Enforce Min Required Fields For Upload";
const char kAutofillEnforceMinRequiredFieldsForUploadDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fillable fields before uploading field-type votes for that form.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillNoLocalSaveOnUnmaskSuccessName[] =
    "Remove the option to save local copies of unmasked server cards";
const char kAutofillNoLocalSaveOnUnmaskSuccessDescription[] =
    "When enabled, the server card unmask prompt will not include the checkbox "
    "to also save the card locally on the current device upon success.";

const char kAutofillNoLocalSaveOnUploadSuccessName[] =
    "Disable saving local copy of uploaded card when credit card upload "
    "succeeds";
const char kAutofillNoLocalSaveOnUploadSuccessDescription[] =
    "When enabled, no local copy of server card will be saved when credit card "
    "upload succeeds.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillShowAllSuggestionsOnPrefilledFormsName[] =
    "Enable showing all suggestions when focusing prefilled field";
const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[] =
    "When enabled: show all suggestions when the focused field value has not "
    "been entered by the user. When disabled: use the field value as a filter.";

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kBrowserContainerKeepsContentViewName[] =
    "Browser Container retains the content view";
const char kBrowserContainerKeepsContentViewDescription[] =
    "When enable, the browser container keeps the content view in the view "
    "hierarchy, to avoid WKWebView from being unloaded from the process.";

const char kBrowserTaskScheduler[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

// TODO(crbug.com/893314) : Remove this flag.
const char kClosingLastIncognitoTabName[] = "Closing Last Incognito Tab";
const char kClosingLastIncognitoTabDescription[] =
    "Automatically switches to the regular tabs panel in the tab grid after "
    "closing the last incognito tab";

const char kCollectionsCardPresentationStyleName[] =
    "Card style presentation for Collections.";
const char kCollectionsCardPresentationStyleDescription[] =
    "When enabled collections are presented using the new iOS13 card "
    "style.";

const char kContextualSearch[] = "Contextual Search";
const char kContextualSearchDescription[] =
    "Whether or not Contextual Search is enabled.";

const char kCopiedContentBehaviorName[] =
    "Enable differentiating between copied urls, text, and images";
const char kCopiedContentBehaviorDescription[] =
    "When enabled, places that handled copied urls (omnibox long-press, toolbar"
    "menus) will differentiate between copied urls, text, and images.";

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kEnableAutocompleteDataRetentionPolicyName[] =
    "Enable automatic cleanup of expired Autocomplete entries.";
const char kEnableAutocompleteDataRetentionPolicyDescription[] =
    "If enabled, will clean-up Autocomplete entries whose last use date is "
    "older than the current retention policy. These entries will be "
    "permanently deleted from the client on startup, and will be unlinked "
    "from sync.";

const char kEnableAutofillCreditCardUploadUpdatePromptExplanationName[] =
    "Enable updated prompt explanation when offering credit card upload";
const char kEnableAutofillCreditCardUploadUpdatePromptExplanationDescription[] =
    "If enabled, changes the server save card prompt's explanation to mention "
    "the saving of the billing address.";

const char kEnableAutofillDoNotUploadSaveUnsupportedCardsName[] =
    "Prevents upload save on cards from unsupported networks";
const char kEnableAutofillDoNotUploadSaveUnsupportedCardsDescription[] =
    "If enabled, cards from unsupported networks will not be offered upload "
    "save, and will instead be offered local save.";

const char kEnableAutofillSaveCardShowNoThanksName[] =
    "Show explicit decline option in credit card save prompts";
const char kEnableAutofillSaveCardShowNoThanksDescription[] =
    "If enabled, adds a [No thanks] button to credit card save prompts.";

const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[] =
    "Enable limit on offering to save the same credit card repeatedly";
const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[] =
    "If enabled, prevents popping up the credit card offer-to-save prompt if "
    "it has repeatedly been ignored, declined, or failed.";

const char kEnableAutofillImportDynamicFormsName[] =
    "Allow credit card import from dynamic forms after entry";
const char kEnableAutofillImportDynamicFormsDescription[] =
    "If enabled, offers credit card save for dynamic forms from the page after "
    "information has been entered into them.";

const char kEnableAutofillImportNonFocusableCreditCardFormsName[] =
    "Allow credit card import from forms that disappear after entry";
const char kEnableAutofillImportNonFocusableCreditCardFormsDescription[] =
    "If enabled, offers credit card save for forms that are hidden from the "
    "page after information has been entered into them, including "
    "accordion-style checkout flows.";

const char kEnableClipboardProviderImageSuggestionsName[] =
    "Enable copied image provider";
const char kEnableClipboardProviderImageSuggestionsDescription[] =
    "Enable suggesting a search for the image copied to the clipboard";

const char kEnableClipboardProviderTextSuggestionsName[] =
    "Enable copied text provider";
const char kEnableClipboardProviderTextSuggestionsDescription[] =
    "Enable suggesting a search for text copied to the clipboard";

const char kEnableSyncUSSBookmarksName[] = "Enable USS for bookmarks sync";
const char kEnableSyncUSSBookmarksDescription[] =
    "Enables the new, experimental implementation of bookmark sync";

const char kEnableSyncUSSPasswordsName[] = "Enable USS for passwords sync";
const char kEnableSyncUSSPasswordsDescription[] =
    "Enables the new, experimental implementation of password sync";

const char kEnableSyncUSSNigoriName[] = "Enable USS for sync encryption keys";
const char kEnableSyncUSSNigoriDescription[] =
    "Enables the new, experimental implementation of sync encryption keys";

const char kFCMInvalidationsName[] =
    "Enable invalidations delivery via new FCM based protocol";
const char kFCMInvalidationsDescription[] =
    "Use the new FCM-based protocol for deliveling invalidations";

const char kFillOnAccountSelectHttpName[] =
    "Fill passwords on account selection on HTTP origins";
const char kFillOnAccountSelectHttpDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load on HTTP origins.";

const char kFindInPageiFrameName[] = "Find in Page in iFrames.";
const char kFindInPageiFrameDescription[] =
    "When enabled, Find In Page will search in iFrames.";

const char kFullscreenViewportAdjustmentExperimentName[] =
    "Fullscreen Viewport Adjustment Mode";
const char kFullscreenViewportAdjustmentExperimentDescription[] =
    "The different ways in which the web view's viewport is updated for scroll "
    "events.  The default option updates the web view's frame.";

const char kHistoryBatchUpdatesFilterName[] = "History Single Batch Filtering";
const char kHistoryBatchUpdatesFilterDescription[] =
    "When enabled History inserts and deletes history items in the same "
    "BatchUpdates block.";

const char kIdentityDiscName[] = "Identity Disc";
const char kIdentityDiscDescription[] =
    "Enables Identity Disc, profile avatar icon button in toolbar.";

const char kIgnoresViewportScaleLimitsName[] = "Ignore Viewport Scale Limits";
const char kIgnoresViewportScaleLimitsDescription[] =
    "When enabled the page can always be scaled, regardless of author intent.";

const char kInfobarUIRebootName[] = "Infobar UI Reboot";
const char kInfobarUIRebootDescription[] =
    "When enabled, Infobar will use the new UI.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kLanguageSettingsName[] = "Language Settings";
const char kLanguageSettingsDescription[] =
    "Enables the Language Settings page allowing modifications to user "
    "preferred languages and translate preferences.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kNewClearBrowsingDataUIName[] = "Clear Browsing Data UI";
const char kNewClearBrowsingDataUIDescription[] =
    "Enable new Clear Browsing Data UI.";

const char kNewOmniboxPopupLayoutName[] = "New omnibox popup";
const char kNewOmniboxPopupLayoutDescription[] =
    "Switches the omnibox suggestions and omnibox itself to display the new "
    "design with favicons, new suggestion layout, rich entity support.";

const char kNonModalDialogsName[] = "Use non-modal JavaScript dialogs";
const char kNonModalDialogsDescription[] =
    "Presents JavaScript dialogs non-modally so that the user can change tabs "
    "while a dialog is displayed.";

const char kOfflineVersionWithoutNativeContentName[] =
    "Use offline pages without native content";
const char kOfflineVersionWithoutNativeContentDescription[] =
    "Shows offline pages directly in the web view.  This feature is forced"
    "enabled if web::features::kSlimNavigationManager is enabled.";

const char kOmniboxPopupShortcutIconsInZeroStateName[] =
    "Show zero-state omnibox shortcuts";
const char kOmniboxPopupShortcutIconsInZeroStateDescription[] =
    "Instead of ZeroSuggest, show most visited sites and collection shortcuts "
    "in the omnibox popup.";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Enable 'switch to this tab' option";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable the 'switch to this tab' options in the omnibox suggestions.";

const char kOmniboxUIElideSuggestionUrlAfterHostName[] =
    "Hide the path, query, and ref of omnibox suggestions";
const char kOmniboxUIElideSuggestionUrlAfterHostDescription[] =
    "Elides the path, query, and ref of suggested URLs in the omnibox "
    "dropdown.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxUseDefaultSearchEngineFaviconName[] =
    "Default search engine favicon in the omnibox";
const char kOmniboxUseDefaultSearchEngineFaviconDescription[] =
    "Shows default search engine favicon in the omnibox";

const char kOmniboxOnDeviceHeadSuggestionsName[] =
    "Omnibox on device head suggestions";
const char kOmniboxOnDeviceHeadSuggestionsDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model";

const char kOptionalArticleThumbnailName[] =
    "Enable optional thumbnails for NTP articles";
const char kOptionalArticleThumbnailDescription[] =
    "Make thumbnails of NTP articles optional due to European copyright "
    "directive(EUCD). Also change the layout of article cells";

const char kPasswordGenerationName[] = "Password generation suggestion";
const char kPasswordGenerationDescription[] =
    "Add 'Suggest Password' in suggestion list for form completion.";

const char kSearchIconToggleName[] = "Change the icon for the search button";
const char kSearchIconToggleDescription[] =
    "Different icons for the search button.";

const char kSendTabToSelfName[] = "Send tab to self";
const char kSendTabToSelfDescription[] =
    "Allows users to receive tabs that were pushed from another of their "
    "synced devices, in order to easily transition tabs between devices.";

const char kSendTabToSelfBroadcastName[] = "Send tab to self broadcast";
const char kSendTabToSelfBroadcastDescription[] =
    "Allows users to broadcast the tab they send to all of their devices "
    "instead of targetting only one device.";

const char kSendTabToSelfShowSendingUIName[] =
    "Send tab to self show sending UI";
const char kSendTabToSelfShowSendingUIDescription[] =
    "Allows users to push tabs from this device to another of their synced "
    "devices, in order to easily transition tabs between them. Requires the "
    "Send tab to self flag to also be enabled.";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSettingsAddPaymentMethodName[] =
    "Enable the add payment method button";
const char kSettingsAddPaymentMethodDescription[] =
    "Allow a user to add a new credit card to payment methods from the "
    "settings menu.";

const char kSettingsRefreshName[] = "Enable the UI Refresh for Settings";
const char kSettingsRefreshDescription[] =
    "Change the UI appearance of the settings to have something in phase with "
    "UI Refresh.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSlimNavigationManagerName[] = "Use Slim Navigation Manager";
const char kSlimNavigationManagerDescription[] =
    "When enabled, uses the experimental slim navigation manager that provides "
    "better compatibility with HTML navigation spec.";

const char kSnapshotDrawViewName[] = "Use DrawViewHierarchy for Snapshots";
const char kSnapshotDrawViewDescription[] =
    "When enabled, snapshots will be taken using |-drawViewHierarchy:|.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncSupportSecondaryAccountName[] =
    "Support secondary accounts for Sync standalone transport";
const char kSyncSupportSecondaryAccountDescription[] =
    "If enabled, allows Chrome Sync to start in standalone transport mode for "
    "a signed-in account that has not been chosen as Chrome's primary account. "
    "This only has an effect if sync-standalone-transport is also enabled.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kToolbarNewTabButtonName[] =
    "Enable New Tab button in the bottom toolbar";
const char kToolbarNewTabButtonDescription[] =
    "When enabled, the bottom toolbar middle button opens a new tab";

const char kTranslateManualTriggerName[] = "Enable manual translate trigger";
const char kTranslateManualTriggerDescription[] =
    "Show a menu item in the popup menu that triggers page translation.";

const char kUnifiedConsentName[] = "Unified Consent";
const char kUnifiedConsentDescription[] =
    "Enables a unified management of user consent for privacy-related "
    "features. This includes new confirmation screens and improved settings "
    "pages.";

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kUseMultiloginEndpointName[] = "Use Multilogin endpoint.";
const char kUseMultiloginEndpointDescription[] =
    "Use Gaia OAuth multilogin for identity consistency.";

const char kUseNSURLSessionForGaiaSigninRequestsName[] =
    "Use NSURLSession for sign-in requests";
const char kUseNSURLSessionForGaiaSigninRequestsDescription[] =
    "Use NSURLSession to make sign-in requests to Gaia";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWebClearBrowsingDataName[] = "Web-API for browsing data";
const char kWebClearBrowsingDataDescription[] =
    "When enabled the Clear Browsing Data feature is using the web API.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

const char kWKHTTPSystemCookieStoreName[] = "Use WKHTTPSystemCookieStore.";
const char kWKHTTPSystemCookieStoreDescription[] =
    "Use WKHTTPCookieStore backed store for main context URL requests.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
