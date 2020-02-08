// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <utility>

#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_handle.h"

namespace previews {

namespace {

// Returns false if the optimization guide decision is false. If unknown, it
// depends on the preview type whether the preview should be applied. Currently,
// only DeferAllScript may be applied in the unknown case.
bool ShouldApplyPreviewWithDecision(
    PreviewsType type,
    optimization_guide::OptimizationGuideDecision decision) {
  switch (decision) {
    case optimization_guide::OptimizationGuideDecision::kFalse:
      return false;
    case optimization_guide::OptimizationGuideDecision::kTrue:
      return true;
    case optimization_guide::OptimizationGuideDecision::kUnknown:
      return type == PreviewsType::DEFER_ALL_SCRIPT
                 ? params::ApplyDeferWhenOptimizationGuideDecisionUnknown()
                 : false;
  }
}

// The default max size of the cache holding resource loading hints by URL.
size_t kDefaultMaxResourceLoadingHintsCacheSize = 10;

// Returns base::nullopt if |previews_type| can't be converted.
base::Optional<optimization_guide::proto::OptimizationType>
ConvertPreviewsTypeToOptimizationType(PreviewsType previews_type) {
  switch (previews_type) {
    case PreviewsType::NONE:
      return optimization_guide::proto::OPTIMIZATION_NONE;
    case PreviewsType::NOSCRIPT:
      return optimization_guide::proto::NOSCRIPT;
    case PreviewsType::UNSPECIFIED:
      return optimization_guide::proto::TYPE_UNSPECIFIED;
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return optimization_guide::proto::RESOURCE_LOADING;
    case PreviewsType::LITE_PAGE_REDIRECT:
      return optimization_guide::proto::LITE_PAGE_REDIRECT;
    case PreviewsType::DEFER_ALL_SCRIPT:
      return optimization_guide::proto::DEFER_ALL_SCRIPT;
    default:
      return base::nullopt;
  }
}

// Returns the optimization types to register with the Optimization Guide
// Decider based on which Previews are enabled for the session.
base::flat_set<optimization_guide::proto::OptimizationType>
GetOptimizationTypesToRegister() {
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types;

  if (params::IsNoScriptPreviewsEnabled())
    optimization_types.insert(optimization_guide::proto::NOSCRIPT);
  if (params::IsResourceLoadingHintsEnabled())
    optimization_types.insert(optimization_guide::proto::RESOURCE_LOADING);
  if (params::IsLitePageServerPreviewsEnabled())
    optimization_types.insert(optimization_guide::proto::LITE_PAGE_REDIRECT);
  if (params::IsDeferAllScriptPreviewsEnabled())
    optimization_types.insert(optimization_guide::proto::DEFER_ALL_SCRIPT);

  return optimization_types;
}

// Parses |resource_loading_hints| and returns a vector of resource patterns
// that can be blocked.
std::vector<std::string> GetResourcePatternsToBlock(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::ResourceLoadingHint>&
        resource_loading_hints) {
  std::vector<std::string> resource_patterns_to_block;
  for (const auto& resource_loading_hint : resource_loading_hints) {
    if (!resource_loading_hint.resource_pattern().empty() &&
        resource_loading_hint.loading_optimization_type() ==
            optimization_guide::proto::LOADING_BLOCK_RESOURCE) {
      resource_patterns_to_block.push_back(
          resource_loading_hint.resource_pattern());
    }
  }
  return resource_patterns_to_block;
}

}  // namespace

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : optimization_guide_decider_(optimization_guide_decider),
      resource_loading_hints_cache_(kDefaultMaxResourceLoadingHintsCacheSize),
      registered_optimization_types_(GetOptimizationTypesToRegister()) {
  DCHECK(optimization_guide_decider_);

  optimization_guide_decider_->RegisterOptimizationTypesAndTargets(
      std::vector<optimization_guide::proto::OptimizationType>(
          registered_optimization_types_.begin(),
          registered_optimization_types_.end()),
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() = default;

bool PreviewsOptimizationGuide::ShouldShowPreview(
    content::NavigationHandle* navigation_handle) {
  // See if we should override the optimization guide and always show a preview.
  if (params::OverrideShouldShowPreviewCheck())
    return true;

  optimization_guide::OptimizationGuideDecision decision =
      optimization_guide_decider_->ShouldTargetNavigation(
          navigation_handle,
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  return decision == optimization_guide::OptimizationGuideDecision::kTrue;
}

bool PreviewsOptimizationGuide::CanApplyPreview(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    PreviewsType type) {
  // See if we need to bypass the lite page redirect blacklist.
  if (type == PreviewsType::LITE_PAGE_REDIRECT &&
      params::LitePageRedirectPreviewIgnoresOptimizationGuideFilter()) {
    return true;
  }

  base::Optional<optimization_guide::proto::OptimizationType>
      optimization_type = ConvertPreviewsTypeToOptimizationType(type);
  if (!optimization_type.has_value())
    return false;

  // See if we can apply the optimization. Note that
  // |optimization_guide_decider_| also ensures that the current browser
  // conditions match a painful page load as a prerequisite for returning true.
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationGuideDecision decision =
      optimization_guide_decider_->CanApplyOptimization(
          navigation_handle, *optimization_type, &optimization_metadata);

  if (!ShouldApplyPreviewWithDecision(type, decision))
    return false;

  // Previews metadata is mostly best effort and not actually required for all
  // previews, so just return early if it's not populated.
  if (!optimization_metadata.previews_metadata())
    return true;

  // If we have metadata, populate information from metadata.
  const optimization_guide::proto::PreviewsMetadata previews_metadata =
      optimization_metadata.previews_metadata().value();
  if (previews_data && previews_metadata.has_inflation_percent()) {
    previews_data->set_data_savings_inflation_percent(
        previews_metadata.inflation_percent());
  }
  if (previews_metadata.resource_loading_hints_size() > 0) {
    resource_loading_hints_cache_.Put(
        navigation_handle->GetURL(),
        GetResourcePatternsToBlock(previews_metadata.resource_loading_hints()));
  }

  return true;
}

bool PreviewsOptimizationGuide::AreCommitTimePreviewsAvailable(
    content::NavigationHandle* navigation_handle) {
  // We use this method as a way of enforcing some sort of preview ordering.
  // Thus, we check if we can potentially apply any of the client-side previews,
  // and if any of them potentially can be applied, then we return true.
  const std::vector<optimization_guide::proto::OptimizationType>
      optimization_types_to_check = {
          optimization_guide::proto::DEFER_ALL_SCRIPT,
          optimization_guide::proto::RESOURCE_LOADING,
          optimization_guide::proto::NOSCRIPT};

  bool might_have_hint = false;
  for (const auto optimization_type : optimization_types_to_check) {
    // Don't check for the hint if the optimization type is not enabled.
    if (registered_optimization_types_.find(optimization_type) ==
        registered_optimization_types_.end()) {
      continue;
    }

    if (optimization_guide_decider_->CanApplyOptimization(
            navigation_handle, optimization_type,
            /*optimization_metadata=*/nullptr) !=
        optimization_guide::OptimizationGuideDecision::kFalse) {
      might_have_hint = true;
      break;
    }
  }
  return might_have_hint;
}

bool PreviewsOptimizationGuide::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) {
  auto rlh_it = resource_loading_hints_cache_.Get(url);
  if (rlh_it == resource_loading_hints_cache_.end())
    return false;

  *out_resource_patterns_to_block = rlh_it->second;
  return true;
}

}  // namespace previews
