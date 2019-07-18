// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/composite_matcher.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
using PageAccess = PermissionsData::PageAccess;
using RedirectAction = CompositeMatcher::RedirectAction;

namespace {

bool AreIDsUnique(const CompositeMatcher::MatcherList& matchers) {
  std::set<size_t> ids;
  for (const auto& matcher : matchers) {
    bool did_insert = ids.insert(matcher->id()).second;
    if (!did_insert)
      return false;
  }

  return true;
}

bool AreSortedPrioritiesUnique(const CompositeMatcher::MatcherList& matchers) {
  base::Optional<size_t> previous_priority;
  for (const auto& matcher : matchers) {
    if (matcher->priority() == previous_priority)
      return false;
    previous_priority = matcher->priority();
  }

  return true;
}

bool HasMatchingAllowRule(const RulesetMatcher* matcher,
                          const RequestParams& params) {
  if (!params.allow_rule_cache.contains(matcher))
    params.allow_rule_cache[matcher] = matcher->HasMatchingAllowRule(params);

  return params.allow_rule_cache[matcher];
}

// Upgrades the url's scheme to HTTPS.
GURL GetUpgradedUrl(const GURL& url) {
  DCHECK(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kFtpScheme));

  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  return url.ReplaceComponents(replacements);
}

// Compares |redirect_rule| and |upgrade_rule| and determines the redirect URL
// based on the rule with the higher priority.
GURL GetUrlByRulePriority(const flat_rule::UrlRule* redirect_rule,
                          const flat_rule::UrlRule* upgrade_rule,
                          const GURL& request_url,
                          GURL redirect_rule_url) {
  DCHECK(upgrade_rule || redirect_rule);

  if (!upgrade_rule)
    return redirect_rule_url;

  if (!redirect_rule)
    return GetUpgradedUrl(request_url);

  return upgrade_rule->priority() > redirect_rule->priority()
             ? GetUpgradedUrl(request_url)
             : redirect_rule_url;
}

}  // namespace

RedirectAction::RedirectAction(base::Optional<GURL> redirect_url,
                               bool notify_request_withheld)
    : redirect_url(std::move(redirect_url)),
      notify_request_withheld(notify_request_withheld) {}

RedirectAction::~RedirectAction() = default;

RedirectAction::RedirectAction(RedirectAction&&) = default;
RedirectAction& RedirectAction::operator=(RedirectAction&& other) = default;

CompositeMatcher::CompositeMatcher(MatcherList matchers)
    : matchers_(std::move(matchers)) {
  SortMatchersByPriority();
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
    SortMatchersByPriority();
  } else {
    // Update the matcher. The priority for a given ID should remain the same.
    DCHECK_EQ(new_matcher->priority(), (*it)->priority());
    *it = std::move(new_matcher);
  }

  // Clear the renderers' cache so that they take the updated rules into
  // account.
  ClearRendererCacheOnNavigation();
  has_any_extra_headers_matcher_.reset();
}

bool CompositeMatcher::ShouldBlockRequest(const RequestParams& params) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldBlockRequestTime."
      "SingleExtension");

  for (const auto& matcher : matchers_) {
    if (HasMatchingAllowRule(matcher.get(), params))
      return false;
    if (matcher->HasMatchingBlockRule(params))
      return true;
  }

  return false;
}

RedirectAction CompositeMatcher::ShouldRedirectRequest(
    const RequestParams& params,
    PageAccess page_access) const {
  // TODO(karandeepb): change this to report time in micro-seconds.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldRedirectRequestTime."
      "SingleExtension");

  bool notify_request_withheld = false;
  for (const auto& matcher : matchers_) {
    if (HasMatchingAllowRule(matcher.get(), params)) {
      return RedirectAction(base::nullopt /* redirect_url */,
                            false /* notify_request_withheld */);
    }

    if (page_access == PageAccess::kAllowed) {
      GURL redirect_rule_url;

      const flat_rule::UrlRule* redirect_rule =
          matcher->GetRedirectRule(params, &redirect_rule_url);
      const flat_rule::UrlRule* upgrade_rule = matcher->GetUpgradeRule(params);

      if (!upgrade_rule && !redirect_rule)
        continue;

      GURL redirect_url =
          GetUrlByRulePriority(redirect_rule, upgrade_rule, *params.url,
                               std::move(redirect_rule_url));
      return RedirectAction(std::move(redirect_url),
                            false /* notify_request_withheld */);
    }

    // If the extension has no host permissions for the request, it can still
    // upgrade the request.
    if (matcher->GetUpgradeRule(params)) {
      return RedirectAction(GetUpgradedUrl(*params.url),
                            false /* notify_request_withheld */);
    }

    GURL redirect_url;
    notify_request_withheld |=
        (page_access == PageAccess::kWithheld &&
         matcher->GetRedirectRule(params, &redirect_url));
  }

  return RedirectAction(base::nullopt /* redirect_url */,
                        notify_request_withheld);
}

uint8_t CompositeMatcher::GetRemoveHeadersMask(const RequestParams& params,
                                               uint8_t current_mask) const {
  uint8_t mask = current_mask;
  for (const auto& matcher : matchers_) {
    // The allow rule will override lower priority remove header rules.
    if (HasMatchingAllowRule(matcher.get(), params))
      return mask;
    mask |= matcher->GetRemoveHeadersMask(params, mask);
  }
  return mask;
}

bool CompositeMatcher::HasAnyExtraHeadersMatcher() const {
  if (!has_any_extra_headers_matcher_.has_value())
    has_any_extra_headers_matcher_ = ComputeHasAnyExtraHeadersMatcher();
  return has_any_extra_headers_matcher_.value();
}

bool CompositeMatcher::ComputeHasAnyExtraHeadersMatcher() const {
  for (const auto& matcher : matchers_) {
    if (matcher->IsExtraHeadersMatcher())
      return true;
  }
  return false;
}

void CompositeMatcher::SortMatchersByPriority() {
  std::sort(matchers_.begin(), matchers_.end(),
            [](const std::unique_ptr<RulesetMatcher>& a,
               const std::unique_ptr<RulesetMatcher>& b) {
              return a->priority() > b->priority();
            });
  DCHECK(AreSortedPrioritiesUnique(matchers_));
}

}  // namespace declarative_net_request
}  // namespace extensions
