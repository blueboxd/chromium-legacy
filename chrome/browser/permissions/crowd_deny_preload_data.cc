// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_preload_data.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

using DomainToReputationMap = CrowdDenyPreloadData::DomainToReputationMap;

// Attempts to load the preload data from |proto_path|, parse it as a serialized
// chrome_browser_crowd_deny::PreloadData message, and index it by domain.
// Returns an empty map is anything goes wrong.
DomainToReputationMap LoadAndParseAndIndexPreloadDataFromDisk(
    const base::FilePath& proto_path) {
  std::string binary_proto;
  if (!base::ReadFileToString(proto_path, &binary_proto))
    return {};

  CrowdDenyPreloadData::PreloadData preload_data;
  if (!preload_data.ParseFromString(binary_proto))
    return {};

  std::vector<DomainToReputationMap::value_type> domain_reputation_pairs;
  domain_reputation_pairs.reserve(preload_data.site_reputations_size());
  for (const auto& site_reputation : preload_data.site_reputations()) {
    domain_reputation_pairs.emplace_back(site_reputation.domain(),
                                         site_reputation);
  }
  return DomainToReputationMap(std::move(domain_reputation_pairs));
}

}  // namespace

CrowdDenyPreloadData::CrowdDenyPreloadData() {
  loading_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

CrowdDenyPreloadData::~CrowdDenyPreloadData() = default;

// static
CrowdDenyPreloadData* CrowdDenyPreloadData::GetInstance() {
  static base::NoDestructor<CrowdDenyPreloadData> instance;
  return instance.get();
}

const CrowdDenyPreloadData::SiteReputation*
CrowdDenyPreloadData::GetReputationDataForSite(
    const url::Origin& origin) const {
  if (origin.scheme() != url::kHttpsScheme)
    return nullptr;

  // For now, do not allow subdomain matches.
  const auto it = domain_to_reputation_map_.find(origin.host());
  if (it == domain_to_reputation_map_.end())
    return nullptr;
  return &it->second;
}

void CrowdDenyPreloadData::LoadFromDisk(const base::FilePath& proto_path) {
  // On failure, LoadAndParseAndIndexPreloadDataFromDisk will return an empty
  // map. Replace the in-memory state with that regardless, so that the stale
  // old data will no longer be used.
  base::PostTaskAndReplyWithResult(
      loading_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadAndParseAndIndexPreloadDataFromDisk, proto_path),
      base::BindOnce(&CrowdDenyPreloadData::set_site_reputations,
                     base::Unretained(this)));
}
