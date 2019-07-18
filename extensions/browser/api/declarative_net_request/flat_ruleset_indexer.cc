// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"

#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;
namespace flat_rule = url_pattern_index::flat;

template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

using FlatStringOffset = FlatOffset<flatbuffers::String>;
using FlatStringListOffset = FlatVectorOffset<flatbuffers::String>;

// Writes to |builder| a flatbuffer vector of shared strings corresponding to
// |vec| and returns the offset to it. If |vec| is empty, returns an empty
// offset.
FlatStringListOffset BuildVectorOfSharedStrings(
    flatbuffers::FlatBufferBuilder* builder,
    const std::vector<std::string>& vec) {
  if (vec.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> offsets;
  offsets.reserve(vec.size());
  for (const auto& str : vec)
    offsets.push_back(builder->CreateSharedString(str));
  return builder->CreateVector(offsets);
}

std::vector<std::unique_ptr<url_pattern_index::UrlPatternIndexBuilder>>
CreateIndexBuilders(flatbuffers::FlatBufferBuilder* builder) {
  std::vector<std::unique_ptr<url_pattern_index::UrlPatternIndexBuilder>>
      result(flat::ActionIndex_count);
  for (size_t i = 0; i < flat::ActionIndex_count; ++i) {
    result[i] =
        std::make_unique<url_pattern_index::UrlPatternIndexBuilder>(builder);
  }
  return result;
}

}  // namespace

FlatRulesetIndexer::FlatRulesetIndexer()
    : index_builders_(CreateIndexBuilders(&builder_)) {}

FlatRulesetIndexer::~FlatRulesetIndexer() = default;

void FlatRulesetIndexer::AddUrlRule(const IndexedRule& indexed_rule) {
  DCHECK(!finished_);

  ++indexed_rules_count_;

  FlatStringListOffset domains_included_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.domains);
  FlatStringListOffset domains_excluded_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.excluded_domains);
  FlatStringOffset url_pattern_offset =
      builder_.CreateSharedString(indexed_rule.url_pattern);

  FlatOffset<flat_rule::UrlRule> offset = flat_rule::CreateUrlRule(
      builder_, indexed_rule.options, indexed_rule.element_types,
      indexed_rule.activation_types, indexed_rule.url_pattern_type,
      indexed_rule.anchor_left, indexed_rule.anchor_right,
      domains_included_offset, domains_excluded_offset, url_pattern_offset,
      indexed_rule.id, indexed_rule.priority);

  std::vector<UrlPatternIndexBuilder*> builders = GetBuilders(indexed_rule);
  DCHECK(!builders.empty());
  for (UrlPatternIndexBuilder* builder : builders)
    builder->IndexUrlRule(offset);

  // Store additional metadata required for a redirect rule.
  if (indexed_rule.action_type == dnr_api::RULE_ACTION_TYPE_REDIRECT) {
    DCHECK(!indexed_rule.redirect_url.empty());
    FlatStringOffset redirect_url_offset =
        builder_.CreateSharedString(indexed_rule.redirect_url);
    metadata_.push_back(flat::CreateUrlRuleMetadata(builder_, indexed_rule.id,
                                                    redirect_url_offset));
  }
}

void FlatRulesetIndexer::Finish() {
  DCHECK(!finished_);
  finished_ = true;

  std::vector<url_pattern_index::UrlPatternIndexOffset> index_offsets;
  index_offsets.reserve(index_builders_.size());
  for (const auto& builder : index_builders_)
    index_offsets.push_back(builder->Finish());

  FlatVectorOffset<url_pattern_index::flat::UrlPatternIndex>
      index_vector_offset = builder_.CreateVector(index_offsets);

  // Store the extension metadata sorted by ID to support fast lookup through
  // binary search.
  FlatVectorOffset<flat::UrlRuleMetadata> extension_metadata_offset =
      builder_.CreateVectorOfSortedTables(&metadata_);

  FlatOffset<flat::ExtensionIndexedRuleset> root_offset =
      flat::CreateExtensionIndexedRuleset(builder_, index_vector_offset,
                                          extension_metadata_offset);
  flat::FinishExtensionIndexedRulesetBuffer(builder_, root_offset);
}

base::span<const uint8_t> FlatRulesetIndexer::GetData() {
  DCHECK(finished_);
  return base::make_span(builder_.GetBufferPointer(), builder_.GetSize());
}

std::vector<FlatRulesetIndexer::UrlPatternIndexBuilder*>
FlatRulesetIndexer::GetBuilders(const IndexedRule& indexed_rule) {
  switch (indexed_rule.action_type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return {index_builders_[flat::ActionIndex_block].get()};
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return {index_builders_[flat::ActionIndex_allow].get()};
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return {index_builders_[flat::ActionIndex_redirect].get()};
    case dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS:
      return GetRemoveHeaderBuilders(indexed_rule.remove_headers_set);
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
      return {index_builders_[flat::ActionIndex_upgrade_scheme].get()};
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return {};
}

std::vector<FlatRulesetIndexer::UrlPatternIndexBuilder*>
FlatRulesetIndexer::GetRemoveHeaderBuilders(
    const std::set<dnr_api::RemoveHeaderType>& types) {
  // A single "removeHeaders" JSON/indexed rule does still correspond to a
  // single flatbuffer rule but can be stored in multiple indices.
  DCHECK(!types.empty());
  std::vector<UrlPatternIndexBuilder*> result;
  for (dnr_api::RemoveHeaderType type : types) {
    switch (type) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        NOTREACHED();
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_cookie_header].get());
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_referer_header].get());
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_set_cookie_header].get());
        break;
    }
  }
  return result;
}

}  // namespace declarative_net_request
}  // namespace extensions
