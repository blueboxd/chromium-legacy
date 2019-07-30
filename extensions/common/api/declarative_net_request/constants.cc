// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

const char kAPIPermission[] = "declarativeNetRequest";
const char kFeedbackAPIPermission[] = "declarativeNetRequestFeedback";

const char kIDKey[] = "id";
const char kPriorityKey[] = "priority";
const char kRuleConditionKey[] = "condition";
const char kRuleActionKey[] = "action";
const char kUrlFilterKey[] = "urlFilter";
const char kIsUrlFilterCaseSensitiveKey[] = "isUrlFilterCaseSensitive";
const char kDomainsKey[] = "domains";
const char kExcludedDomainsKey[] = "excludedDomains";
const char kResourceTypesKey[] = "resourceTypes";
const char kExcludedResourceTypesKey[] = "excludedResourceTypes";
const char kDomainTypeKey[] = "domainType";
const char kRuleActionTypeKey[] = "type";
const char kRedirectUrlKey[] = "redirectUrl";
const char kRemoveHeadersListKey[] = "removeHeadersList";
const char kRedirectPath[] = "action.redirect";
const char kExtensionPathPath[] = "action.redirect.extensionPath";
const char kTransformSchemePath[] = "action.redirect.transform.scheme";
const char kTransformPortPath[] = "action.redirect.transform.port";
const char kTransformQueryPath[] = "action.redirect.transform.query";
const char kTransformFragmentPath[] = "action.redirect.transform.fragment";
const char kTransformQueryTransformPath[] =
    "action.redirect.transform.queryTransform";

}  // namespace declarative_net_request
}  // namespace extensions
