// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
using PageAccess = PermissionsData::PageAccess;
using ActionInfo = CompositeMatcher::ActionInfo;

namespace {

bool AreIDsUnique(const CompositeMatcher::MatcherList& matchers) {
  std::set<RulesetID> ids;
  for (const auto& matcher : matchers) {
    bool did_insert = ids.insert(matcher->id()).second;
    if (!did_insert)
      return false;
  }

  return true;
}

// Helper to log the time taken in CompositeMatcher::GetBeforeRequestAction.
class ScopedGetBeforeRequestActionTimer {
 public:
  ScopedGetBeforeRequestActionTimer() = default;
  ~ScopedGetBeforeRequestActionTimer() {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest.EvaluateBeforeRequestTime."
        "SingleExtension2",
        timer_.Elapsed(), base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromMilliseconds(50), 50);
  }

 private:
  base::ElapsedTimer timer_;
};

}  // namespace

ActionInfo::ActionInfo(base::Optional<RequestAction> action,
                       bool notify_request_withheld)
    : action(std::move(action)),
      notify_request_withheld(notify_request_withheld) {}

ActionInfo::~ActionInfo() = default;

ActionInfo::ActionInfo(ActionInfo&&) = default;
ActionInfo& ActionInfo::operator=(ActionInfo&& other) = default;

CompositeMatcher::CompositeMatcher(MatcherList matchers)
    : matchers_(std::move(matchers)) {
  DCHECK(AreIDsUnique(matchers_));
}

CompositeMatcher::~CompositeMatcher() = default;

void CompositeMatcher::AddOrUpdateRuleset(
    std::unique_ptr<RulesetMatcher> new_matcher) {
  // A linear search is ok since the number of rulesets per extension is
  // expected to be quite small.
  auto it = std::find_if(
      matchers_.begin(), matchers_.end(),
      [&new_matcher](const std::unique_ptr<RulesetMatcher>& matcher) {
        return new_matcher->id() == matcher->id();
      });

  if (it == matchers_.end()) {
    matchers_.push_back(std::move(new_matcher));
  } else {
    // Update the matcher.
    *it = std::move(new_matcher);
  }

  // Clear the renderers' cache so that they take the updated rules into
  // account.
  ClearRendererCacheOnNavigation();
  has_any_extra_headers_matcher_.reset();
}

ActionInfo CompositeMatcher::GetBeforeRequestAction(
    const RequestParams& params,
    PageAccess page_access) const {
  ScopedGetBeforeRequestActionTimer timer;

  bool notify_request_withheld = false;
  base::Optional<RequestAction> final_action;
  for (const auto& matcher : matchers_) {
    base::Optional<RequestAction> action =
        matcher->GetBeforeRequestAction(params);
    params.allow_rule_cache[matcher.get()] =
        action && action->IsAllowOrAllowAllRequests();

    if (action && action->type == RequestAction::Type::REDIRECT) {
      // Redirecting requires host permissions.
      // TODO(crbug.com/1033780): returning base::nullopt here results in
      // counterintuitive behavior.
      if (page_access == PageAccess::kDenied) {
        action = base::nullopt;
      } else if (page_access == PageAccess::kWithheld) {
        action = base::nullopt;
        notify_request_withheld = true;
      }
    }

    final_action =
        GetMaxPriorityAction(std::move(final_action), std::move(action));
  }

  if (final_action)
    return ActionInfo(std::move(final_action), false);
  return ActionInfo(base::nullopt, notify_request_withheld);
}

uint8_t CompositeMatcher::GetRemoveHeadersMask(
    const RequestParams& params,
    uint8_t excluded_remove_headers_mask,
    std::vector<RequestAction>* remove_headers_actions) const {
  uint8_t mask = 0;
  for (const auto& matcher : matchers_) {
    // An allow or allowAllRequests rule will override remove header rules
    // within |matcher|.
    if (!params.allow_rule_cache.contains(matcher.get())) {
      // GetBeforeRequestAction is normally called before GetRemoveHeadersMask,
      // so this should never happen in non-test builds. There are tests that
      // call GetRemoveHeadersMask directly, though.
      base::Optional<RequestAction> action =
          matcher->GetBeforeRequestAction(params);
      params.allow_rule_cache[matcher.get()] =
          action && action->IsAllowOrAllowAllRequests();
    }
    if (params.allow_rule_cache[matcher.get()])
      return 0;

    mask |= matcher->GetRemoveHeadersMask(
        params, mask | excluded_remove_headers_mask, remove_headers_actions);
  }

  DCHECK(!(mask & excluded_remove_headers_mask));
  return mask;
}

std::vector<RequestAction> CompositeMatcher::GetModifyHeadersActions(
    const RequestParams& params) const {
  std::vector<RequestAction> modify_headers_actions;

  for (const auto& matcher : matchers_) {
    // TODO(crbug.com/947591): An allow or allowAllRequests rule should override
    // all equal or lower priority modifyHeaders rules specified by |matcher|.
    DCHECK(params.allow_rule_cache.contains(matcher.get()));
    if (params.allow_rule_cache[matcher.get()])
      return std::vector<RequestAction>();

    std::vector<RequestAction> actions_for_matcher =
        matcher->GetModifyHeadersActions(params);

    modify_headers_actions.insert(
        modify_headers_actions.end(),
        std::make_move_iterator(actions_for_matcher.begin()),
        std::make_move_iterator(actions_for_matcher.end()));
  }

  // Sort |modify_headers_actions| in descending order of priority.
  std::sort(modify_headers_actions.begin(), modify_headers_actions.end(),
            [](const RequestAction& lhs, const RequestAction& rhs) {
              return lhs.index_priority > rhs.index_priority;
            });
  return modify_headers_actions;
}

bool CompositeMatcher::HasAnyExtraHeadersMatcher() const {
  if (!has_any_extra_headers_matcher_.has_value())
    has_any_extra_headers_matcher_ = ComputeHasAnyExtraHeadersMatcher();
  return has_any_extra_headers_matcher_.value();
}

void CompositeMatcher::OnRenderFrameCreated(content::RenderFrameHost* host) {
  for (auto& matcher : matchers_)
    matcher->OnRenderFrameCreated(host);
}

void CompositeMatcher::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  for (auto& matcher : matchers_)
    matcher->OnRenderFrameDeleted(host);
}

void CompositeMatcher::OnDidFinishNavigation(content::RenderFrameHost* host) {
  for (auto& matcher : matchers_)
    matcher->OnDidFinishNavigation(host);
}

bool CompositeMatcher::ComputeHasAnyExtraHeadersMatcher() const {
  for (const auto& matcher : matchers_) {
    if (matcher->IsExtraHeadersMatcher())
      return true;
  }
  return false;
}

}  // namespace declarative_net_request
}  // namespace extensions
