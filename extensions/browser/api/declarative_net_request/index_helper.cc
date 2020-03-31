// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/index_helper.h"

#include <utility>

#include "base/barrier_closure.h"

namespace extensions {
namespace declarative_net_request {

// static
void IndexHelper::Start(std::vector<RulesetSource> sources,
                        IndexCallback callback) {
  // Note we use ref-counting instead of manual memory management since there
  // are some subtle cases:
  //  - Zero rulesets to index.
  //  - All individual callbacks return synchronously.
  // In these cases there's a potential for a use-after-free with manual memory
  // management.
  auto index_helper = base::WrapRefCounted(
      new IndexHelper(std::move(sources), std::move(callback)));
  index_helper->Start();
}

IndexHelper::IndexHelper(std::vector<RulesetSource> sources,
                         IndexCallback callback)
    : sources_(std::move(sources)), callback_(std::move(callback)) {}

IndexHelper::~IndexHelper() = default;

void IndexHelper::Start() {
  // |all_done_closure| will be invoked once |barrier_closure| is run
  // |sources_.size()| times.
  base::OnceClosure all_done_closure =
      base::BindOnce(&IndexHelper::OnAllRulesetsIndexed, this);
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(sources_.size(), std::move(all_done_closure));

  for (const RulesetSource& source : sources_) {
    auto callback =
        base::BindOnce(&IndexHelper::OnRulesetIndexed, this, barrier_closure);
    source.IndexAndPersistJSONRuleset(&decoder_, std::move(callback));
  }
}

void IndexHelper::OnAllRulesetsIndexed() {
  DCHECK_EQ(sources_.size(), results_.size());

  // Our job is done.
  std::move(callback_).Run(std::move(results_));
}

// Callback invoked when indexing of a single ruleset is completed.
void IndexHelper::OnRulesetIndexed(base::OnceClosure ruleset_done_closure,
                                   IndexAndPersistJSONRulesetResult result) {
  results_.push_back(std::move(result));
  std::move(ruleset_done_closure).Run();
}

}  // namespace declarative_net_request
}  // namespace extensions
