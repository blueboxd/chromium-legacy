// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#include "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/infobars/core/infobar_feature.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_state/core/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/ios/features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/find_in_page/features.h"
#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#include "ios/chrome/browser/ui/settings/autofill/features.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/common/web_view_creation_util.h"

#if !defined(OFFICIAL_BUILD)
#include "components/variations/variations_switches.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using flags_ui::FeatureEntry;

namespace {

const FeatureEntry::FeatureParam kMarkHttpAsDangerous[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerous}};
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam kMarkHttpAsDangerWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerWarning}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     base::size(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     base::size(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a grey triangle icon)", kMarkHttpAsDangerWarning,
     base::size(kMarkHttpAsDangerWarning), nullptr}};

const FeatureEntry::Choice kAutofillIOSDelayBetweenFieldsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"0", autofill::switches::kAutofillIOSDelayBetweenFields, "0"},
    {"10", autofill::switches::kAutofillIOSDelayBetweenFields, "10"},
    {"20", autofill::switches::kAutofillIOSDelayBetweenFields, "20"},
    {"50", autofill::switches::kAutofillIOSDelayBetweenFields, "50"},
    {"100", autofill::switches::kAutofillIOSDelayBetweenFields, "100"},
    {"200", autofill::switches::kAutofillIOSDelayBetweenFields, "200"},
    {"500", autofill::switches::kAutofillIOSDelayBetweenFields, "500"},
    {"1000", autofill::switches::kAutofillIOSDelayBetweenFields, "1000"},
};

const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestIncognitoExperimentVariations[] = {{
        "relevance-1000",
        (FeatureEntry::FeatureParam[]){
            {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
             "1000"}},
        1,
        nullptr,
    }};

const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations[] = {
        {
            "relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            2,
            nullptr,
        },
        {
            "request-delay-100ms",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "100"}},
            1,
            nullptr,
        },
        {
            "delay-100ms-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "100"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        },
        {
            "request-delay-200ms",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "200"}},
            1,
            nullptr,
        },
        {
            "delay-200ms-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "200"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        }};

const FeatureEntry::FeatureParam kOmniboxNTPZPSRemote[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:8:*", "RemoteNoUrl"}};

const FeatureEntry::FeatureVariation kOmniboxOnFocusSuggestionsVariations[] = {
    {"ZPS on NTP: Remote History", kOmniboxNTPZPSRemote,
     base::size(kOmniboxNTPZPSRemote), "t3316653" /* variation_id */},
};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         base::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         base::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         base::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         base::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         base::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         base::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         base::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowAll[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowAll}};
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowOne[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowOne}};

const FeatureEntry::FeatureVariation
    kAutofillUseMobileLabelDisambiguationVariations[] = {
        {"(show all)", kAutofillUseMobileLabelDisambiguationShowAll,
         base::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         base::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};

// To add a new entry, add to the end of kFeatureEntries. There are four
// distinct types of entries:
// . ENABLE_DISABLE_VALUE: entry is either enabled, disabled, or uses the
//   default value for this feature. Use the ENABLE_DISABLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// . FEATURE_VALUE: entry is associated with a base::Feature instance. Entry is
//   either enabled, disabled, or uses the default value of the associated
//   base::Feature instance. To specify this type of entry use the macro
//   FEATURE_VALUE_TYPE supplying it the base::Feature instance.
// . FEATURE_WITH_PARAM_VALUES: a list of choices associated with a
//   base::Feature instance. Choices corresponding to the default state, a
//   universally enabled state, and a universally disabled state are
//   automatically included. To specify this type of entry use the macro
//   FEATURE_WITH_PARAMS_VALUE_TYPE supplying it the base::Feature instance and
//   the array of choices.
//
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    {"enable-mark-http-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         security_state::features::kMarkHttpAsFeature,
         kMarkHttpAsFeatureVariations,
         "MarkHttpAs")},
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"drag_and_drop", flag_descriptions::kDragAndDropName,
     flag_descriptions::kDragAndDropDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDragAndDrop)},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
    {"enable-sync-device-info-in-transport-mode",
     flag_descriptions::kSyncDeviceInfoInTransportModeName,
     flag_descriptions::kSyncDeviceInfoInTransportModeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncDeviceInfoInTransportMode)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"autofill-ios-delay-between-fields",
     flag_descriptions::kAutofillIOSDelayBetweenFieldsName,
     flag_descriptions::kAutofillIOSDelayBetweenFieldsDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kAutofillIOSDelayBetweenFieldsChoices)},
    {"autofill-show-all-profiles-on-prefilled-forms",
     flag_descriptions::kAutofillShowAllSuggestionsOnPrefilledFormsName,
     flag_descriptions::kAutofillShowAllSuggestionsOnPrefilledFormsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillShowAllSuggestionsOnPrefilledForms)},
    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},
    {"autofill-rich-metadata-queries",
     flag_descriptions::kAutofillRichMetadataQueriesName,
     flag_descriptions::kAutofillRichMetadataQueriesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRichMetadataQueries)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(fullscreen::features::kSmoothScrollingDefault)},
    {"autofill-enforce-min-required-fields-for-heuristics",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForHeuristicsName,
     flag_descriptions::
         kAutofillEnforceMinRequiredFieldsForHeuristicsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics)},
    {"autofill-enforce-min-required-fields-for-query",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForQuery)},
    {"autofill-enforce-min-required-fields-for-upload",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForUpload)},
    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},
    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},
    {"webpage-text-accessibility",
     flag_descriptions::kWebPageTextAccessibilityName,
     flag_descriptions::kWebPageTextAccessibilityDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextAccessibility)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderIncognito,
         kOmniboxOnDeviceHeadSuggestIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadSuggestIncognitoIOS")},
    {"omnibox-on-device-head-suggestions-non-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderNonIncognito,
         kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadSuggestNonIncognitoIOS")},
    {"omnibox-on-focus-suggestions",
     flag_descriptions::kOmniboxOnFocusSuggestionsName,
     flag_descriptions::kOmniboxOnFocusSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOnFocusSuggestions,
                                    kOmniboxOnFocusSuggestionsVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"infobar-ui-reboot", flag_descriptions::kInfobarUIRebootName,
     flag_descriptions::kInfobarUIRebootDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSInfobarUIReboot)},
    {"find-in-page-iframe", flag_descriptions::kFindInPageiFrameName,
     flag_descriptions::kFindInPageiFrameDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFindInPageiFrame)},
    {"enable-clipboard-provider-image-suggestions",
     flag_descriptions::kEnableClipboardProviderImageSuggestionsName,
     flag_descriptions::kEnableClipboardProviderImageSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kEnableClipboardProviderImageSuggestions)},
    {"snapshot-draw-view", flag_descriptions::kSnapshotDrawViewName,
     flag_descriptions::kSnapshotDrawViewDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotDrawView)},
#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)
    {"settings-refresh", flag_descriptions::kSettingsRefreshName,
     flag_descriptions::kSettingsRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSettingsRefresh)},
    {"send-uma-cellular", flag_descriptions::kSendUmaOverAnyNetwork,
     flag_descriptions::kSendUmaOverAnyNetworkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUmaCellular)},
    {"autofill-no-local-save-on-unmask-success",
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUnmaskSuccess)},
    {"omnibox-preserve-default-match-against-async-update",
     flag_descriptions::kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName,
     flag_descriptions::
         kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         omnibox::kOmniboxPreserveDefaultMatchAgainstAsyncUpdate)},
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"autofill-prune-suggestions",
     flag_descriptions::kAutofillPruneSuggestionsName,
     flag_descriptions::kAutofillPruneSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPruneSuggestions)},
    {"enable-sync-trusted-vault",
     flag_descriptions::kEnableSyncTrustedVaultName,
     flag_descriptions::kEnableSyncTrustedVaultDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncSupportTrustedVaultPassphrase)},
    {"enable-sync-uss-nigori", flag_descriptions::kEnableSyncUSSNigoriName,
     flag_descriptions::kEnableSyncUSSNigoriDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncUSSNigori)},
    {"collections-card-presentation-style",
     flag_descriptions::kCollectionsCardPresentationStyleName,
     flag_descriptions::kCollectionsCardPresentationStyleDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCollectionsCardPresentationStyle)},
    {"enable-autofill-credit-card-upload-editable-expiration-date",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableExpirationDate)},
    {"credit-card-scanner", flag_descriptions::kCreditCardScannerName,
     flag_descriptions::kCreditCardScannerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCreditCardScanner)},
    {"ios-breadcrumbs", flag_descriptions::kLogBreadcrumbsName,
     flag_descriptions::kLogBreadcrumbsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLogBreadcrumbs)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kForceStartupSigninPromo)},
    {"embedder-block-restore-url",
     flag_descriptions::kEmbedderBlockRestoreUrlName,
     flag_descriptions::kEmbedderBlockRestoreUrlDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEmbedderBlockRestoreUrl)},
    {"messages-confirm-infobars",
     flag_descriptions::kConfirmInfobarMessagesUIName,
     flag_descriptions::kConfirmInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kConfirmInfobarMessagesUI)},
    {"disable-animation-on-low-battery",
     flag_descriptions::kDisableAnimationOnLowBatteryName,
     flag_descriptions::kDisableAnimationOnLowBatteryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableAnimationOnLowBattery)},
    {"messages-save-card-infobar",
     flag_descriptions::kSaveCardInfobarMessagesUIName,
     flag_descriptions::kSaveCardInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSaveCardInfobarMessagesUI)},
    {"messages-translate-infobar",
     flag_descriptions::kTranslateInfobarMessagesUIName,
     flag_descriptions::kTranslateInfobarMessagesUIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kTranslateInfobarMessagesUI)},
    {"autofill-save-card-dismiss-on-navigation",
     flag_descriptions::kAutofillSaveCardDismissOnNavigationName,
     flag_descriptions::kAutofillSaveCardDismissOnNavigationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardDismissOnNavigation)},
    {"enable-persistent-downloads",
     flag_descriptions::kEnablePersistentDownloadsName,
     flag_descriptions::kEnablePersistentDownloadsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnablePersistentDownloads)},
    {"force-unstacked-tabstrip", flag_descriptions::kForceUnstackedTabstripName,
     flag_descriptions::kForceUnstackedTabstripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kForceUnstackedTabstrip)},
    {"use-js-error-page", flag_descriptions::kUseJSForErrorPageName,
     flag_descriptions::kUseJSForErrorPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseJSForErrorPage)},
    {"messages-download-infobar",
     flag_descriptions::kDownloadInfobarMessagesUIName,
     flag_descriptions::kDownloadInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadInfobarMessagesUI)},
    {"desktop-version-default", flag_descriptions::kDefaultToDesktopOnIPadName,
     flag_descriptions::kDefaultToDesktopOnIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseDefaultUserAgentInWebClient)},
    {"mobile-google-srp", flag_descriptions::kMobileGoogleSRPName,
     flag_descriptions::kMobileGoogleSRPDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kMobileGoogleSRP)},
    {"messages-crash-restore-infobars",
     flag_descriptions::kCrashRestoreInfobarMessagesUIName,
     flag_descriptions::kCrashRestoreInfobarMessagesUIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCrashRestoreInfobarMessagesUI)},
    {"infobar-overlay-ui", flag_descriptions::kInfobarOverlayUIName,
     flag_descriptions::kInfobarOverlayUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kInfobarOverlayUI)},
    {"autofill-save-card-infobar-edit-support",
     flag_descriptions::kAutofillSaveCardInfobarEditSupportName,
     flag_descriptions::kAutofillSaveCardInfobarEditSupportDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardInfobarEditSupport)},
    {"reload-sad-tab", flag_descriptions::kReloadSadTabName,
     flag_descriptions::kReloadSadTabDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kReloadSadTab)},
    {"improved-cookie-controls",
     flag_descriptions::kImprovedCookieControlsDescription,
     flag_descriptions::kImprovedCookieControlsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(content_settings::kImprovedCookieControls)},
    {"page-info-refactoring", flag_descriptions::kPageInfoRefactoringName,
     flag_descriptions::kPageInfoRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageInfoRefactoring)},
    {"contained-browser-bvc", flag_descriptions::kContainedBVCName,
     flag_descriptions::kContainedBVCDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContainedBVC)},
    {"clear-synced-data", flag_descriptions::kClearSyncedDataName,
     flag_descriptions::kClearSyncedDataDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kClearSyncedData)},
    {"ssl-committed-interstitials",
     flag_descriptions::kSSLCommittedInterstitialsName,
     flag_descriptions::kSSLCommittedInterstitialsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSSLCommittedInterstitials)},
    {"change-tab-switcher-position",
     flag_descriptions::kChangeTabSwitcherPositionName,
     flag_descriptions::kChangeTabSwitcherPositionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kChangeTabSwitcherPosition)},
    {"fullscreen-controller-browser-scoped",
     flag_descriptions::kFullscreenControllerBrowserScopedName,
     flag_descriptions::kFullscreenControllerBrowserScopedDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         fullscreen::features::kFullscreenControllerBrowserScoped)},
    {"open-downloads-in-files.app",
     flag_descriptions::kOpenDownloadsInFilesAppName,
     flag_descriptions::kOpenDownloadsInFilesAppDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOpenDownloadsInFilesApp)},
    {"ios-lookalike-url-navigation-suggestions-ui",
     flag_descriptions::kIOSLookalikeUrlNavigationSuggestionsUIName,
     flag_descriptions::kIOSLookalikeUrlNavigationSuggestionsUIDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         web::features::kIOSLookalikeUrlNavigationSuggestionsUI)},
    {"safe-browsing-available", flag_descriptions::kSafeBrowsingAvailableName,
     flag_descriptions::kSafeBrowsingAvailableDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kSafeBrowsingAvailableOnIOS)},
    {"new-signin-architecture", flag_descriptions::kNewSigninArchitectureName,
     flag_descriptions::kNewSigninArchitectureDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewSigninArchitecture)},
    {"qr-code-generation", flag_descriptions::kQRCodeGenerationName,
     flag_descriptions::kQRCodeGenerationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kQRCodeGeneration)},
    {"autofill-enable-surfacing-server-card-nickname",
     flag_descriptions::kAutofillEnableSurfacingServerCardNicknameName,
     flag_descriptions::kAutofillEnableSurfacingServerCardNicknameDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSurfacingServerCardNickname)},
    {"managed-bookmarks-ios", flag_descriptions::kManagedBookmarksIOSName,
     flag_descriptions::kManagedBookmarksIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kManagedBookmarksIOS)},
    {"enable-autofill-cache-server-card-info",
     flag_descriptions::kEnableAutofillCacheServerCardInfoName,
     flag_descriptions::kEnableAutofillCacheServerCardInfoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheServerCardInfo)},
    {"infobar-ui-reboot-only-ios13",
     flag_descriptions::kInfobarUIRebootOnlyiOS13Name,
     flag_descriptions::kInfobarUIRebootOnlyiOS13Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kInfobarUIRebootOnlyiOS13)},
    {"edit-bookmarks-ios", flag_descriptions::kEditBookmarksIOSName,
     flag_descriptions::kEditBookmarksIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEditBookmarksIOS)},
    {"url-blocklist-ios", flag_descriptions::kURLBlocklistIOSName,
     flag_descriptions::kURLBlocklistIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kURLBlocklistIOS)},
#if defined(__IPHONE_13_4)
    {"pointer-support", flag_descriptions::kPointerSupportName,
     flag_descriptions::kPointerSupportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPointerSupport)},
#endif  // defined(__IPHONE_13_4)
    {"autofill-enable-google-issued-card",
     flag_descriptions::kAutofillEnableGoogleIssuedCardName,
     flag_descriptions::kAutofillEnableGoogleIssuedCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableGoogleIssuedCard)},
    {"enable-mygoogle", flag_descriptions::kEnableMyGoogleName,
     flag_descriptions::kEnableMyGoogleDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableMyGoogle)},
};

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
}  // namespace

// Add all switches from experimental flags to |command_line|.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the UA flag if UseMobileSafariUA is enabled.
  if ([defaults boolForKey:@"UseMobileSafariUA"]) {
    // Safari uses "Vesion/", followed by the OS version excluding bugfix, where
    // Chrome puts its product token.
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    std::string product = base::StringPrintf("Version/%d.%d", major, minor);

    command_line->AppendSwitchASCII(
        switches::kUserAgent,
        web::BuildUserAgentFromProduct(web::UserAgentType::MOBILE, product));
  }

  // Freeform commandline flags.  These are added last, so that any flags added
  // earlier in this function take precedence.
  if ([defaults boolForKey:@"EnableFreeformCommandLineFlags"]) {
    base::CommandLine::StringVector flags;
    // Append an empty "program" argument.
    flags.push_back("");

    // The number of flags corresponds to the number of text fields in
    // Experimental.plist.
    const int kNumFreeformFlags = 5;
    for (int i = 1; i <= kNumFreeformFlags; ++i) {
      NSString* key =
          [NSString stringWithFormat:@"FreeformCommandLineFlag%d", i];
      NSString* flag = [defaults stringForKey:key];
      if ([flag length]) {
        // iOS keyboard replaces -- with —, so undo that.
        flag = [flag stringByReplacingOccurrencesOfString:@"—"
                                               withString:@"--"
                                                  options:0
                                                    range:NSMakeRange(0, 1)];
        // To make things easier, allow flags with no dashes by prepending them
        // here. This also allows for flags that just have one dash if they
        // exist.
        if (![flag hasPrefix:@"-"]) {
          flag = [@"--" stringByAppendingString:flag];
        }
        flags.push_back(base::SysNSStringToUTF8(flag));
      }
    }

    base::CommandLine temp_command_line(flags);
    command_line->AppendArguments(temp_command_line, false);
  }

  // Populate command line flag for 3rd party keyboard omnibox workaround.
  NSString* enableThirdPartyKeyboardWorkaround =
      [defaults stringForKey:@"EnableThirdPartyKeyboardWorkaround"];
  if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableThirdPartyKeyboardWorkaround);
  } else if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableThirdPartyKeyboardWorkaround);
  }

  ios::GetChromeBrowserProvider()->AppendSwitchesFromExperimentalSettings(
      defaults, command_line);
}

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  GetGlobalFlagsState().ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  // Occasionally DCHECK crashes on canary can be very distuptive.  An
  // experimental flag was added to aid in temporarily disabling this for
  // canary testers.
#if defined(DCHECK_IS_CONFIGURABLE)
  if (experimental_flags::AreDCHECKCrashesDisabled()) {
    std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
    overrides.push_back(
        {std::cref(base::kDCheckIsFatalFeature),
         base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
    feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  }
#endif  // defined(DCHECK_IS_CONFIGURABLE)

  return GetGlobalFlagsState().RegisterAllFeatureVariationParameters(
      flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  GetGlobalFlagsState().GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  GetGlobalFlagsState().SetFeatureEntryEnabled(flags_storage, internal_name,
                                               enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  GetGlobalFlagsState().ResetAllFlags(flags_storage);
}

namespace testing {

const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing
