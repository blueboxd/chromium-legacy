// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RulesMonitorService>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

bool RulesetInfoCompareByID(const RulesetInfo& lhs, const RulesetInfo& rhs) {
  return lhs.source().id() < rhs.source().id();
}

}  // namespace

// Helper to bridge tasks to FileSequenceHelper. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_helper_(std::make_unique<FileSequenceHelper>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_helper_));
  }

  void LoadRulesets(
      LoadRequestData load_data,
      FileSequenceHelper::LoadRulesetsUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_helper_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceHelper::LoadRulesets,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

  void UpdateDynamicRules(
      LoadRequestData load_data,
      std::vector<int> rule_ids_to_remove,
      std::vector<dnr_api::Rule> rules_to_add,
      FileSequenceHelper::UpdateDynamicRulesUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |update_dynamic_rules_task| is run.
    base::OnceClosure update_dynamic_rules_task =
        base::BindOnce(&FileSequenceHelper::UpdateDynamicRules,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(rule_ids_to_remove),
                       std::move(rules_to_add), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE,
                                std::move(update_dynamic_rules_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceHelper> file_sequence_helper_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceBridge);
};

// static
BrowserContextKeyedAPIFactory<RulesMonitorService>*
RulesMonitorService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
std::unique_ptr<RulesMonitorService>
RulesMonitorService::CreateInstanceForTesting(
    content::BrowserContext* context) {
  return base::WrapUnique(new RulesMonitorService(context));
}

// static
RulesMonitorService* RulesMonitorService::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<RulesMonitorService>::Get(
      browser_context);
}

void RulesMonitorService::UpdateDynamicRules(
    const Extension& extension,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    DynamicRuleUpdateUICallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  DynamicRuleUpdate update(std::move(rule_ids_to_remove),
                           std::move(rules_to_add), std::move(callback));

  // There are two possible cases, either the extension has completed its
  // initial ruleset load in response to OnExtensionLoaded or it is still
  // undergoing that load. For the latter case, we must wait till the ruleset
  // loading is complete.
  auto it = pending_dynamic_rule_updates_.find(extension.id());
  if (it != pending_dynamic_rule_updates_.end()) {
    it->second.push_back(std::move(update));
    return;
  }

  // Else we can update dynamic rules immediately.
  UpdateDynamicRulesInternal(extension.id(), std::move(update));
}

RulesMonitorService::DynamicRuleUpdate::DynamicRuleUpdate(
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    RulesMonitorService::DynamicRuleUpdateUICallback ui_callback)
    : rule_ids_to_remove(std::move(rule_ids_to_remove)),
      rules_to_add(std::move(rules_to_add)),
      ui_callback(std::move(ui_callback)) {}

RulesMonitorService::DynamicRuleUpdate::DynamicRuleUpdate(DynamicRuleUpdate&&) =
    default;
RulesMonitorService::DynamicRuleUpdate&
RulesMonitorService::DynamicRuleUpdate::operator=(DynamicRuleUpdate&&) =
    default;
RulesMonitorService::DynamicRuleUpdate::~DynamicRuleUpdate() = default;

RulesMonitorService::RulesMonitorService(
    content::BrowserContext* browser_context)
    : file_sequence_bridge_(std::make_unique<FileSequenceBridge>()),
      prefs_(ExtensionPrefs::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      context_(browser_context),
      ruleset_manager_(browser_context),
      action_tracker_(browser_context) {
  // Don't monitor extension lifecycle if the API is not available. This is
  // useful since we base some of our actions (like loading dynamic ruleset on
  // extension load) on the presence of certain extension prefs. These may still
  // be remaining from an earlier install on which the feature was available.
  if (IsAPIAvailable())
    registry_observer_.Add(extension_registry_);
}

RulesMonitorService::~RulesMonitorService() = default;

/* Description of thread hops for various scenarios:

   On ruleset load success:
      - UI -> File -> UI.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset load failure:
      - UI -> File -> UI.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset unload:
      - UI.

   On dynamic rules update.
      - UI -> File -> UI -> IPC to extension
*/

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK_EQ(context_, browser_context);

  LoadRequestData load_data(extension->id());
  int expected_ruleset_checksum;

  // Static rulesets.
  {
    std::vector<RulesetSource> sources =
        RulesetSource::CreateStatic(*extension);
    bool ruleset_failed_to_load = false;
    for (auto& source : sources) {
      if (!source.enabled())
        continue;

      if (!prefs_->GetDNRStaticRulesetChecksum(extension->id(), source.id(),
                                               &expected_ruleset_checksum)) {
        // This might happen on prefs corruption.
        ruleset_failed_to_load = true;
        continue;
      }

      RulesetInfo static_ruleset(std::move(source));
      static_ruleset.set_expected_checksum(expected_ruleset_checksum);
      load_data.rulesets.push_back(std::move(static_ruleset));
    }

    if (ruleset_failed_to_load) {
      warning_service_->AddWarnings(
          {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
    }
  }

  // Dynamic ruleset
  if (prefs_->GetDNRDynamicRulesetChecksum(extension->id(),
                                           &expected_ruleset_checksum)) {
    RulesetInfo dynamic_ruleset(
        RulesetSource::CreateDynamic(browser_context, extension->id()));
    dynamic_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(dynamic_ruleset));
  }

  // Sort by ruleset IDs. This would ensure the dynamic ruleset comes first
  // followed by static rulesets, which would be in the order in which they were
  // defined in the manifest.
  std::sort(load_data.rulesets.begin(), load_data.rulesets.end(),
            &RulesetInfoCompareByID);

  if (load_data.rulesets.empty()) {
    if (test_observer_)
      test_observer_->OnRulesetLoadComplete(extension->id());

    return;
  }

  // Add an entry for the extension in |pending_dynamic_rule_updates_| to
  // indicate that it's loading its rulesets.
  bool inserted =
      pending_dynamic_rule_updates_
          .emplace(extension->id(), std::vector<DynamicRuleUpdate>())
          .second;
  DCHECK(inserted);

  auto load_ruleset_callback = base::BindOnce(
      &RulesMonitorService::OnRulesetsLoaded, weak_factory_.GetWeakPtr());
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(context_, browser_context);

  // Return early if the extension does not have an active indexed ruleset.
  if (!ruleset_manager_.GetMatcherForExtension(extension->id()))
    return;

  UnloadRulesets(extension->id());
}

void RulesMonitorService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(context_, browser_context);

  // Skip if the extension will be reinstalled soon.
  if (reason == UNINSTALL_REASON_REINSTALL)
    return;

  // Skip if the extension doesn't have a dynamic ruleset.
  int dynamic_checksum;
  if (!prefs_->GetDNRDynamicRulesetChecksum(extension->id(),
                                            &dynamic_checksum)) {
    return;
  }

  // Cleanup the dynamic rules directory for the extension.
  // TODO(karandeepb): It's possible that this task fails, e.g. during shutdown.
  // Make this more robust.
  RulesetSource source =
      RulesetSource::CreateDynamic(browser_context, extension->id());
  DCHECK_EQ(source.json_path().DirName(), source.indexed_path().DirName());
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     source.json_path().DirName(), false /* recursive */));
}

void RulesMonitorService::UpdateDynamicRulesInternal(
    const ExtensionId& extension_id,
    DynamicRuleUpdate update) {
  if (!extension_registry_->enabled_extensions().Contains(extension_id)) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related book-keeping is
    // done.
    std::move(update.ui_callback).Run(base::nullopt /* error */);
    return;
  }

  LoadRequestData data(extension_id);

  // We are updating the indexed ruleset. Don't set the expected checksum since
  // it'll change.
  data.rulesets.emplace_back(
      RulesetSource::CreateDynamic(context_, extension_id));

  auto update_rules_callback =
      base::BindOnce(&RulesMonitorService::OnDynamicRulesUpdated,
                     weak_factory_.GetWeakPtr(), std::move(update.ui_callback));
  file_sequence_bridge_->UpdateDynamicRules(
      std::move(data), std::move(update.rule_ids_to_remove),
      std::move(update.rules_to_add), std::move(update_rules_callback));
}

void RulesMonitorService::OnRulesetsLoaded(LoadRequestData load_data) {
  DCHECK(!load_data.rulesets.empty());
  DCHECK(std::all_of(
      load_data.rulesets.begin(), load_data.rulesets.end(),
      [](const RulesetInfo& ruleset) { return ruleset.source().enabled(); }));
  DCHECK(std::is_sorted(load_data.rulesets.begin(), load_data.rulesets.end(),
                        &RulesetInfoCompareByID));

  // Perform pending dynamic rule updates.
  {
    auto it = pending_dynamic_rule_updates_.find(load_data.extension_id);

    // Even if there are no updates to perform (i.e., the list is empty), we
    // expect an entry in the map.
    DCHECK(it != pending_dynamic_rule_updates_.end());

    for (DynamicRuleUpdate& update : it->second)
      UpdateDynamicRulesInternal(load_data.extension_id, std::move(update));

    pending_dynamic_rule_updates_.erase(it);
  }

  if (test_observer_)
    test_observer_->OnRulesetLoadComplete(load_data.extension_id);

  // The extension may have been uninstalled by this point. Return early if
  // that's the case.
  if (!extension_registry_->GetInstalledExtension(load_data.extension_id))
    return;

  // Update checksums for all rulesets.
  // Note: We also do this for a non-enabled extension. The ruleset on the disk
  // has already been modified at this point. So we do want to update the
  // checksum for it to be in sync with what's on disk.
  for (const RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.new_checksum())
      continue;

    if (ruleset.source().is_dynamic_ruleset()) {
      prefs_->SetDNRDynamicRulesetChecksum(load_data.extension_id,
                                           *(ruleset.new_checksum()));
    } else {
      prefs_->SetDNRStaticRulesetChecksum(load_data.extension_id,
                                          ruleset.source().id(),
                                          *(ruleset.new_checksum()));
    }
  }

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    return;
  }

  // Build the CompositeMatcher for the extension. Also enforce rules limit
  // across the enabled static rulesets. Note: we don't enforce the rules limit
  // at install time (by raising a hard error) to maintain forwards
  // compatibility. Since we iterate based on the ruleset ID, we'll give more
  // preference to rulesets occurring first in the manifest.
  CompositeMatcher::MatcherList matchers;
  size_t static_rules_count = 0;
  size_t static_regex_rules_count = 0;
  bool notify_ruleset_failed_to_load = false;
  for (RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.did_load_successfully()) {
      notify_ruleset_failed_to_load = true;
      continue;
    }

    std::unique_ptr<RulesetMatcher> matcher = ruleset.TakeMatcher();

    // Per-ruleset limits should have been enforced during
    // indexing/installation.
    DCHECK_LE(matcher->GetRegexRulesCount(),
              static_cast<size_t>(dnr_api::MAX_NUMBER_OF_REGEX_RULES));
    DCHECK_LE(matcher->GetRulesCount(), ruleset.source().rule_count_limit());

    if (ruleset.source().is_dynamic_ruleset()) {
      matchers.push_back(std::move(matcher));
      continue;
    }

    size_t new_rules_count = static_rules_count + matcher->GetRulesCount();
    if (new_rules_count > static_cast<size_t>(dnr_api::MAX_NUMBER_OF_RULES))
      continue;

    size_t new_regex_rules_count =
        static_regex_rules_count + matcher->GetRegexRulesCount();
    if (new_regex_rules_count >
        static_cast<size_t>(dnr_api::MAX_NUMBER_OF_REGEX_RULES)) {
      continue;
    }

    static_rules_count = new_rules_count;
    static_regex_rules_count = new_regex_rules_count;
    matchers.push_back(std::move(matcher));
  }

  if (notify_ruleset_failed_to_load) {
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
  }

  if (matchers.empty())
    return;

  LoadRulesets(load_data.extension_id,
               std::make_unique<CompositeMatcher>(std::move(matchers)));
}

void RulesMonitorService::OnDynamicRulesUpdated(
    DynamicRuleUpdateUICallback callback,
    LoadRequestData load_data,
    base::Optional<std::string> error) {
  DCHECK_EQ(1u, load_data.rulesets.size());

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK_EQ(dynamic_ruleset.did_load_successfully(), !error.has_value());

  // The extension may have been uninstalled by this point. Return early if
  // that's the case.
  if (!extension_registry_->GetInstalledExtension(load_data.extension_id)) {
    // Still dispatch the |callback|, although it's probably a no-op.
    std::move(callback).Run(std::move(error));
    return;
  }

  // Update the ruleset checksums if needed. Note it's possible that
  // new_checksum() is valid while did_load_successfully() returns false below.
  // This should be rare but can happen when updating the rulesets succeeds but
  // we fail to create a RulesetMatcher from the indexed ruleset file (e.g. due
  // to a file read error). We still update the prefs checksum to ensure the
  // next ruleset load succeeds.
  // Note: We also do this for a non-enabled extension. The ruleset on the disk
  // has already been modified at this point. So we do want to update the
  // checksum for it to be in sync with what's on disk.
  if (dynamic_ruleset.new_checksum()) {
    prefs_->SetDNRDynamicRulesetChecksum(load_data.extension_id,
                                         *dynamic_ruleset.new_checksum());
  }

  // Respond to the extension.
  std::move(callback).Run(std::move(error));

  if (!dynamic_ruleset.did_load_successfully())
    return;

  DCHECK(dynamic_ruleset.new_checksum());

  // It's possible that the extension has been disabled since the initial update
  // rule request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    return;
  }

  // Update the dynamic ruleset.
  UpdateRuleset(load_data.extension_id, dynamic_ruleset.TakeMatcher());
}

void RulesMonitorService::UnloadRulesets(const ExtensionId& extension_id) {
  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  ruleset_manager_.RemoveRuleset(extension_id);
  action_tracker_.ClearExtensionData(extension_id);

  if (had_extra_headers_matcher &&
      !ruleset_manager_.HasAnyExtraHeadersMatcher()) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->DecrementExtraHeadersListenerCount(context_);
  }
}

void RulesMonitorService::LoadRulesets(
    const ExtensionId& extension_id,
    std::unique_ptr<CompositeMatcher> matcher) {
  bool increment_extra_headers =
      !ruleset_manager_.HasAnyExtraHeadersMatcher() &&
      matcher->HasAnyExtraHeadersMatcher();
  ruleset_manager_.AddRuleset(extension_id, std::move(matcher));

  if (increment_extra_headers) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->IncrementExtraHeadersListenerCount(context_);
  }
}

void RulesMonitorService::UpdateRuleset(
    const ExtensionId& extension_id,
    std::unique_ptr<RulesetMatcher> ruleset_matcher) {
  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();

  CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(extension_id);

  // The extension didn't have any active rulesets.
  if (!matcher) {
    CompositeMatcher::MatcherList matchers;
    matchers.push_back(std::move(ruleset_matcher));
    LoadRulesets(extension_id,
                 std::make_unique<CompositeMatcher>(std::move(matchers)));
    return;
  }

  matcher->AddOrUpdateRuleset(std::move(ruleset_matcher));

  bool has_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  if (had_extra_headers_matcher == has_extra_headers_matcher)
    return;
  if (has_extra_headers_matcher) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->IncrementExtraHeadersListenerCount(context_);
  } else {
    ExtensionWebRequestEventRouter::GetInstance()
        ->DecrementExtraHeadersListenerCount(context_);
  }
}

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::
    DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(WarningServiceFactory::GetInstance());
  DependsOn(PermissionHelper::GetFactoryInstance());
}

}  // namespace extensions
