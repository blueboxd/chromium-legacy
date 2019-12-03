// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {

namespace {

base::Optional<RequestAction> GetMaxPriorityAction(
    base::Optional<RequestAction> lhs,
    base::Optional<RequestAction> rhs) {
  if (!lhs)
    return rhs;
  if (!rhs)
    return lhs;
  return lhs->rule_priority > rhs->rule_priority ? std::move(lhs)
                                                 : std::move(rhs);
}

}  // namespace

// static
RulesetMatcher::LoadRulesetResult RulesetMatcher::CreateVerifiedMatcher(
    const RulesetSource& source,
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) {
  DCHECK(matcher);
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;

  if (!base::PathExists(source.indexed_path()))
    return kLoadErrorInvalidPath;

  std::string ruleset_data;
  if (!base::ReadFileToString(source.indexed_path(), &ruleset_data))
    return kLoadErrorFileRead;

  if (!StripVersionHeaderAndParseVersion(&ruleset_data))
    return kLoadErrorVersionMismatch;

  // This guarantees that no memory access will end up outside the buffer.
  if (!IsValidRulesetData(
          base::make_span(reinterpret_cast<const uint8_t*>(ruleset_data.data()),
                          ruleset_data.size()),
          expected_ruleset_checksum)) {
    return kLoadErrorChecksumMismatch;
  }

  UMA_HISTOGRAM_TIMES(
      "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
      timer.Elapsed());

  // Using WrapUnique instead of make_unique since this class has a private
  // constructor.
  *matcher = base::WrapUnique(new RulesetMatcher(
      std::move(ruleset_data), source.id(), source.priority(), source.type(),
      source.extension_id()));
  return kLoadSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

base::Optional<RequestAction> RulesetMatcher::GetBlockOrCollapseAction(
    const RequestParams& params) const {
  base::Optional<RequestAction> action =
      url_pattern_index_matcher_.GetBlockOrCollapseAction(params);
  if (!action)
    action = regex_matcher_.GetBlockOrCollapseAction(params);
  return action;
}

base::Optional<RequestAction> RulesetMatcher::GetAllowAction(
    const RequestParams& params) const {
  base::Optional<RequestAction> action =
      url_pattern_index_matcher_.GetAllowAction(params);
  if (!action)
    action = regex_matcher_.GetAllowAction(params);
  return action;
}

base::Optional<RequestAction> RulesetMatcher::GetRedirectAction(
    const RequestParams& params) const {
  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetRedirectAction(params),
      regex_matcher_.GetRedirectAction(params));
}

base::Optional<RequestAction> RulesetMatcher::GetUpgradeAction(
    const RequestParams& params) const {
  if (!IsUpgradeableRequest(params))
    return base::nullopt;

  return GetMaxPriorityAction(
      url_pattern_index_matcher_.GetUpgradeAction(params),
      regex_matcher_.GetUpgradeAction(params));
}

uint8_t RulesetMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t excluded_remove_headers_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  DCHECK(remove_headers_actions);
  static_assert(
      flat::RemoveHeaderType_ANY <= std::numeric_limits<uint8_t>::max(),
      "flat::RemoveHeaderType can't fit in a uint8_t");

  uint8_t mask = url_pattern_index_matcher_.GetRemoveHeadersMask(
      params, excluded_remove_headers_mask, remove_headers_actions);
  return mask | regex_matcher_.GetRemoveHeadersMask(
                    params, excluded_remove_headers_mask | mask,
                    remove_headers_actions);
}

bool RulesetMatcher::IsExtraHeadersMatcher() const {
  return url_pattern_index_matcher_.IsExtraHeadersMatcher() ||
         regex_matcher_.IsExtraHeadersMatcher();
}

base::Optional<RequestAction>
RulesetMatcher::GetRedirectOrUpgradeActionByPriority(
    const RequestParams& params) const {
  return GetMaxPriorityAction(GetRedirectAction(params),
                              GetUpgradeAction(params));
}

RulesetMatcher::RulesetMatcher(
    std::string ruleset_data,
    size_t id,
    size_t priority,
    api::declarative_net_request::SourceType source_type,
    const ExtensionId& extension_id)
    : RulesetMatcherBase(extension_id, source_type),
      ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      id_(id),
      priority_(priority),
      url_pattern_index_matcher_(extension_id,
                                 source_type,
                                 root_->index_list(),
                                 root_->extension_metadata()),
      regex_matcher_(extension_id,
                     source_type,
                     root_->regex_rules(),
                     root_->extension_metadata()) {}

}  // namespace declarative_net_request
}  // namespace extensions
