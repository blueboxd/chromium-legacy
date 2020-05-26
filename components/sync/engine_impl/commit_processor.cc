// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_processor.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/commit_contributor.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using TypeToIndexMap = std::map<ModelType, size_t>;

CommitProcessor::CommitProcessor(ModelTypeSet commit_types,
                                 CommitContributorMap* commit_contributor_map)
    : commit_types_(commit_types),
      commit_contributor_map_(commit_contributor_map),
      gathered_all_contributions_(false) {
  // NIGORI contributions must be collected in every commit cycle.
  DCHECK(commit_types_.Has(NIGORI));
  DCHECK(commit_contributor_map);
}

CommitProcessor::~CommitProcessor() {}

Commit::ContributionMap CommitProcessor::GatherCommitContributions(
    size_t max_entries,
    bool cookie_jar_mismatch,
    bool cookie_jar_empty) {
  if (gathered_all_contributions_) {
    return Commit::ContributionMap();
  }

  ModelTypeSet contributing_commit_types = commit_types_;
  Commit::ContributionMap contributions;
  size_t num_entries = 0;

  // NIGORI should be committed before any other datatype.
  num_entries +=
      GatherCommitContributionsForType(NIGORI, max_entries, cookie_jar_mismatch,
                                       cookie_jar_empty, &contributions);

  // Next, gather contributions from priority types.
  const size_t num_priority_entries = GatherCommitContributionsForTypes(
      Intersection(contributing_commit_types, PriorityUserTypes()),
      max_entries - num_entries, cookie_jar_mismatch, cookie_jar_empty,
      &contributions);
  num_entries += num_priority_entries;
  DCHECK_LE(num_entries, max_entries);

  // If no commit is needed for NIGORI or priority types, gather commit
  // contributions for the remaining datatypes. This requirement achieves two
  // things:
  // 1. No encrypted data gets committed before the corresponding NIGORI commit,
  //    which can otherwise leave to data loss if the commit fails partially.
  // 2. Regular datatypes do not add latency to priority type commits.
  if (num_entries == 0) {
    num_entries += GatherCommitContributionsForTypes(
        Difference(contributing_commit_types,
                   Union(PriorityUserTypes(), ModelTypeSet(NIGORI))),
        max_entries, cookie_jar_mismatch, cookie_jar_empty, &contributions);
    DCHECK_LE(num_entries, max_entries);
  }

  if (contributing_commit_types == commit_types_ && num_entries < max_entries) {
    // Technically |num_entries| == |max_entries| may also mean that all
    // contributions have been gathered, but it's safe to ignore, since this
    // will lead to empty contribution in the next call and exiting commit
    // cycle (or additional commit cycle in rare cases when new contributions
    // come meanwhile).
    gathered_all_contributions_ = true;
  }

  return contributions;
}

size_t CommitProcessor::GatherCommitContributionsForType(
    ModelType type,
    size_t max_entries,
    bool cookie_jar_mismatch,
    bool cookie_jar_empty,
    Commit::ContributionMap* contributions) {
  auto cm_it = commit_contributor_map_->find(type);
  if (cm_it == commit_contributor_map_->end()) {
    DLOG(ERROR) << "Could not find requested type " << ModelTypeToString(type)
                << " in contributor map.";
    return 0;
  }

  std::unique_ptr<CommitContribution> contribution =
      cm_it->second->GetContribution(max_entries);
  if (!contribution) {
    return 0;
  }

  size_t num_entries = contribution->GetNumEntries();
  DCHECK_LE(num_entries, max_entries);
  contributions->emplace(type, std::move(contribution));

  if (type == SESSIONS) {
    UMA_HISTOGRAM_BOOLEAN("Sync.CookieJarMatchOnNavigation",
                          !cookie_jar_mismatch);
    if (cookie_jar_mismatch) {
      UMA_HISTOGRAM_BOOLEAN("Sync.CookieJarEmptyOnMismatch", cookie_jar_empty);
    }
  }

  return num_entries;
}

size_t CommitProcessor::GatherCommitContributionsForTypes(
    ModelTypeSet types,
    size_t max_entries,
    bool cookie_jar_mismatch,
    bool cookie_jar_empty,
    Commit::ContributionMap* contributions) {
  size_t num_entries = 0;
  for (ModelType type : types) {
    num_entries += GatherCommitContributionsForType(
        type, max_entries - num_entries, cookie_jar_mismatch, cookie_jar_empty,
        contributions);
    if (num_entries >= max_entries) {
      DCHECK_EQ(num_entries, max_entries)
          << "Number of commit entries exceeds maximum";
      break;
    }
  }
  return num_entries;
}

}  // namespace syncer
