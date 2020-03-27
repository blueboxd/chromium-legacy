// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/constants.h"

#include "extensions/common/constants.h"
#include "url/url_constants.h"

namespace extensions {
namespace declarative_net_request {

const char* const kAllowedTransformSchemes[4] = {
    url::kHttpScheme, url::kHttpsScheme, url::kFtpScheme,
    extensions::kExtensionScheme};

const char kErrorResourceTypeDuplicated[] =
    "Rule with id * includes and excludes the same resource.";
const char kErrorInvalidRuleKey[] =
    "Rule with id * has an invalid value for * key. This should be greater "
    "than or equal to *.";
const char kErrorEmptyRulePriority[] =
    "Rule with id * does not specify the value for priority key.";
const char kErrorNoApplicableResourceTypes[] =
    "Rule with id * is not applicable to any resource type.";
const char kErrorEmptyList[] =
    "Rule with id * cannot have an empty list as the value for * key.";
const char kErrorEmptyKey[] =
    "Rule with id * cannot have an empty value for * key.";
const char kErrorInvalidRedirectUrl[] =
    "Rule with id * does not provide a valid URL for * key.";
const char kErrorDuplicateIDs[] = "Rule with id * does not have a unique ID.";
// Don't surface the actual error to the user, since it's an implementation
// detail.
const char kErrorPersisting[] = "Internal error while parsing rules.";
const char kErrorNonAscii[] =
    "Rule with id * cannot have non-ascii characters as part of \"*\" key.";
const char kErrorEmptyRemoveHeadersList[] =
    "Rule with id * does not specify the value for \"*\" key. This is required "
    "for \"removeHeaders\" rules.";
const char kErrorInvalidKey[] =
    "Rule with id * specifies an incorrect value for the \"*\" key.";
const char kErrorInvalidTransformScheme[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Allowed "
    "values are: [*].";
const char kErrorQueryAndTransformBothSpecified[] =
    "Rule with id * cannot specify both \"*\" and \"*\" keys.";
const char kErrorJavascriptRedirect[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Redirects "
    "to javascript urls are not supported.";
const char kErrorMultipleFilters[] =
    "Rule with id * can only specify one of \"*\" or \"*\" keys.";
const char kErrorRegexSubstitutionWithoutFilter[] =
    "Rule with id * can't specify the \"*\" key without specifying the \"*\" "
    "key.";
const char kErrorInvalidAllowAllRequestsResourceType[] =
    "Rule with id * is an \"allowAllRequests\" rule and must specify the "
    "\"resourceTypes\" key. It may only include the \"main_frame\" and "
    "\"sub_frame\" resource types.";
const char kErrorRegexTooLarge[] =
    "Rule with id * specified a more complex regex than allowed as part of the "
    "\"*\" key.";
const char kErrorRegexesTooLarge[] =
    "Rules with ids [*] specified a more complex regex than allowed as part of "
    "the \"*\" key.";
const char kErrorNoHeaderListsSpecified[] =
    "Rule with id * does not specify a value for \"*\" or \"*\" key. At least "
    "one of these keys must be specified with a non-empty list.";
const char kErrorInvalidHeaderName[] =
    "Rule with id * must specify a valid header name to be modified.";
const char kErrorListNotPassed[] = "Rules file must contain a list.";

const char kRuleCountExceeded[] =
    "Rule count exceeded. Some rules were ignored.";
const char kRegexRuleCountExceeded[] =
    "Regex rule count exceeded. Some rules were ignored.";
const char kRuleNotParsedWarning[] =
    "Rule with * couldn't be parsed. Parse error: *.";
const char kTooManyParseFailuresWarning[] =
    "Too many rule parse failures; Reporting the first *.";
const char kInternalErrorUpdatingDynamicRules[] =
    "Internal error while updating dynamic rules.";
const char kInternalErrorGettingDynamicRules[] =
    "Internal error while getting dynamic rules.";
const char kDynamicRuleCountExceeded[] = "Dynamic rule count exceeded.";
const char kDynamicRegexRuleCountExceeded[] =
    "Dynamic rule count for regex rules exceeded.";
const char kIndexAndPersistRulesTimeHistogram[] =
    "Extensions.DeclarativeNetRequest.IndexAndPersistRulesTime";
const char kManifestRulesCountHistogram[] =
    "Extensions.DeclarativeNetRequest.ManifestRulesCount";
const char kUpdateDynamicRulesStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.UpdateDynamicRulesStatus";
const char kReadDynamicRulesJSONStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.ReadDynamicRulesJSONStatus";
const char kIsLargeRegexHistogram[] =
    "Extensions.DeclarativeNetRequest.IsLargeRegexRule";

const char kActionCountPlaceholderBadgeText[] =
    "<<declarativeNetRequestActionCount>>";

const char kErrorGetMatchedRulesMissingPermissions[] =
    "The extension must have the declarativeNetRequestFeedback permission or "
    "have activeTab granted for the specified tab ID in order to call this "
    "function.";

}  // namespace declarative_net_request
}  // namespace extensions
