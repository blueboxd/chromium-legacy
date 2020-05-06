// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

namespace declarative_net_request {

class CompositeMatcher;
class RulesetSource;
class RulesetMatcher;
struct TestRule;

// Enum specifying the extension load type. Used for parameterized tests.
enum class ExtensionLoadType {
  PACKED,
  UNPACKED,
};

// Factory method to construct a RequestAction given a RequestAction type and
// optionally, an ExtensionId.
RequestAction CreateRequestActionForTesting(
    RequestAction::Type type,
    uint32_t rule_id = kMinValidID,
    uint32_t rule_priority = kDefaultPriority,
    RulesetID ruleset_id = kMinValidStaticRulesetID,
    const ExtensionId& extension_id = "extensionid");

// Test helpers for help with gtest expectations and assertions.
bool operator==(const RequestAction& lhs, const RequestAction& rhs);
std::ostream& operator<<(std::ostream& output, RequestAction::Type type);
std::ostream& operator<<(std::ostream& output, const RequestAction& action);
std::ostream& operator<<(std::ostream& output, const ParseResult& result);
std::ostream& operator<<(std::ostream& output,
                         const base::Optional<RequestAction>& action);

// Returns true if the given extension's indexed static rulesets are all valid.
// Should be called on a sequence where file IO is allowed.
bool AreAllIndexedStaticRulesetsValid(const Extension& extension,
                                      content::BrowserContext* browser_context);

// Helper to create a verified ruleset matcher. Populates |matcher| and
// |expected_checksum|. Returns true on success.
bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const RulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum = nullptr);

// Helper to return a RulesetSource bound to temporary files.
RulesetSource CreateTemporarySource(RulesetID id = kMinValidStaticRulesetID,
                                    size_t rule_count_limit = 100,
                                    ExtensionId extension_id = "extensionid");

api::declarative_net_request::ModifyHeaderInfo CreateModifyHeaderInfo(
    api::declarative_net_request::HeaderOperation operation,
    std::string header);

bool EqualsForTesting(
    const api::declarative_net_request::ModifyHeaderInfo& lhs,
    const api::declarative_net_request::ModifyHeaderInfo& rhs);

// Returns the public ruleset IDs corresponding to the given |extension| and
// |matcher|.
std::vector<std::string> GetPublicRulesetIDs(const Extension& extension,
                                             const CompositeMatcher& matcher);

// Test observer for RulesetManager. This is a multi-use observer i.e.
// WaitForExtensionsWithRulesetsCount can be called multiple times per lifetime
// of an observer.
class RulesetManagerObserver : public RulesetManager::TestObserver {
 public:
  explicit RulesetManagerObserver(RulesetManager* manager);
  RulesetManagerObserver(const RulesetManagerObserver&) = delete;
  RulesetManagerObserver& operator=(const RulesetManagerObserver&) = delete;
  ~RulesetManagerObserver() override;

  // Returns the requests seen by RulesetManager since the last call to this
  // function.
  std::vector<GURL> GetAndResetRequestSeen();

  // Waits for the number of rulesets to change to |count|. Note |count| is the
  // number of extensions with rulesets or the number of active
  // CompositeMatchers.
  void WaitForExtensionsWithRulesetsCount(size_t count);

 private:
  // RulesetManager::TestObserver implementation.
  void OnRulesetCountChanged(size_t count) override;
  void OnEvaluateRequest(const WebRequestInfo& request,
                         bool is_incognito_context) override;

  RulesetManager* const manager_;
  size_t current_count_ = 0;
  base::Optional<size_t> expected_count_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<GURL> observed_requests_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
