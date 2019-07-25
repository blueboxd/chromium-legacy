// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints.h"

#include <unordered_set>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hint_update_data.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/previews/core/previews_features.h"
#include "url/gurl.h"

namespace previews {

namespace {

// Name of sentinel file to guard potential crash loops while processing
// the config into hints. It holds the version of the config that is/was
// being processed into hints.
const base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("previews_config_sentinel.txt");

// Creates the sentinel file (at |sentinel_path|) to persistently mark the
// beginning of processing the configuration data for Previews hints. It
// records the configuration version in the file. Returns true when the
// sentinel file is successfully created and processing should continue.
// Returns false if the processing should not continue because the
// file exists with the same version (indicating that processing that version
// failed previously (possibly crash or shutdown). Should be run in the
// background (e.g., same task as PreviewsHints::Create()).
bool CreateSentinelFile(const base::FilePath& sentinel_path,
                        const base::Version& version) {
  DCHECK(version.IsValid());

  if (base::PathExists(sentinel_path)) {
    // Processing apparently did not complete previously, check its version.
    std::string content;
    if (!base::ReadFileToString(sentinel_path, &content)) {
      DLOG(WARNING) << "Error reading previews config sentinel file";
      // Attempt to delete sentinel for fresh start next time.
      base::DeleteFile(sentinel_path, false /* recursive */);
      return false;
    }
    base::Version previous_attempted_version(content);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in previews config sentinel file";
      // Attempt to delete sentinel for fresh start next time.
      base::DeleteFile(sentinel_path, false /* recursive */);
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version in the sentinel file.
  std::string new_sentinel_value = version.GetString();
  if (base::WriteFile(sentinel_path, new_sentinel_value.data(),
                      new_sentinel_value.length()) <= 0) {
    DLOG(ERROR) << "Failed to create sentinel file " << sentinel_path;
    return false;
  }
  return true;
}

// Deletes the sentinel file. This should be done once processing the
// configuration is complete and should be done in the background (e.g.,
// same task as Hints.CreateFromConfig).
void DeleteSentinelFile(const base::FilePath& sentinel_path) {
  if (!base::DeleteFile(sentinel_path, false /* recursive */)) {
    DLOG(ERROR) << "Error deleting sentinel file";
  }
}

// Returns base::nullopt if |optimization_type| can't be converted.
base::Optional<PreviewsType> ConvertProtoOptimizationTypeToPreviewsType(
    optimization_guide::proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::TYPE_UNSPECIFIED:
      return base::nullopt;
    case optimization_guide::proto::NOSCRIPT:
      return PreviewsType::NOSCRIPT;
    case optimization_guide::proto::RESOURCE_LOADING:
      return PreviewsType::RESOURCE_LOADING_HINTS;
    case optimization_guide::proto::LITE_PAGE_REDIRECT:
      return PreviewsType::LITE_PAGE_REDIRECT;
    case optimization_guide::proto::OPTIMIZATION_NONE:
      return PreviewsType::NONE;
    case optimization_guide::proto::DEFER_ALL_SCRIPT:
      return PreviewsType::DEFER_ALL_SCRIPT;
  }
}

// Returns whether the optimization type is enabled on this client.
bool IsEnabledOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::TYPE_UNSPECIFIED:
      return false;
    case optimization_guide::proto::NOSCRIPT:
      return previews::params::IsNoScriptPreviewsEnabled();
    case optimization_guide::proto::RESOURCE_LOADING:
      return previews::params::IsResourceLoadingHintsEnabled();
    case optimization_guide::proto::LITE_PAGE_REDIRECT:
      return previews::params::IsLitePageServerPreviewsEnabled();
    case optimization_guide::proto::OPTIMIZATION_NONE:
      // Always consider enabled to allow as no-op optimization.
      return true;
    case optimization_guide::proto::DEFER_ALL_SCRIPT:
      return previews::params::IsDeferAllScriptPreviewsEnabled();
  }
}

net::EffectiveConnectionType ConvertProtoEffectiveConnectionType(
    optimization_guide::proto::EffectiveConnectionType proto_ect) {
  switch (proto_ect) {
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_OFFLINE:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE;
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_2G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G;
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_3G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G;
    case optimization_guide::proto::EffectiveConnectionType::
        EFFECTIVE_CONNECTION_TYPE_4G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G;
  }
}

}  // namespace

PreviewsHints::PreviewsHints(
    std::unique_ptr<optimization_guide::HintUpdateData> component_update_data)
    : hint_cache_(nullptr),
      component_update_data_(std::move(component_update_data)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PreviewsHints::~PreviewsHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<PreviewsHints> PreviewsHints::CreateFromHintsComponent(
    const optimization_guide::HintsComponentInfo& info,
    std::unique_ptr<optimization_guide::HintUpdateData> component_update_data) {
  optimization_guide::ProcessHintsComponentResult result;
  std::unique_ptr<optimization_guide::proto::Configuration> config =
      ProcessHintsComponent(info, &result);
  if (!config) {
    RecordProcessHintsComponentResult(result);
    return nullptr;
  }

  base::FilePath sentinel_path(info.path.DirName().Append(kSentinelFileName));
  if (!CreateSentinelFile(sentinel_path, info.version)) {
    RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing);
    return nullptr;
  }

  std::unique_ptr<PreviewsHints> hints = CreateFromHintsConfiguration(
      std::move(config), std::move(component_update_data));

  // Completed processing hints data without crashing so clear sentinel.
  DeleteSentinelFile(sentinel_path);
  return hints;
}

// static
std::unique_ptr<PreviewsHints> PreviewsHints::CreateFromHintsConfiguration(
    std::unique_ptr<optimization_guide::proto::Configuration> config,
    std::unique_ptr<optimization_guide::HintUpdateData> component_update_data) {
  optimization_guide::ProcessHintsComponentResult process_hints_result =
      optimization_guide::ProcessHintsComponentResult::kSkippedProcessingHints;
  if (component_update_data) {
    // Process the hints within the configuration. This will move the hints from
    // |config| into |component_update_data|.
    bool did_process_hints = ProcessHints(config.get()->mutable_hints(),
                                          component_update_data.get());
    process_hints_result =
        did_process_hints
            ? optimization_guide::ProcessHintsComponentResult::kSuccess
            : optimization_guide::ProcessHintsComponentResult::
                  kProcessedNoHints;
  }

  // Construct the PrevewsHints object with |component_update_data|, which
  // will later be used to update the HintCache's component data during
  // Initialize().
  std::unique_ptr<PreviewsHints> hints(
      new PreviewsHints(std::move(component_update_data)));

  // Extract any supported large scale blacklists from the configuration.
  hints->ParseOptimizationFilters(*config);

  // Now that processing is complete, record the result.
  optimization_guide::RecordProcessHintsComponentResult(process_hints_result);

  return hints;
}

void PreviewsHints::Initialize(optimization_guide::HintCache* hint_cache,
                               base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hint_cache_);
  DCHECK(hint_cache);
  hint_cache_ = hint_cache;
  if (component_update_data_) {
    hint_cache_->UpdateComponentHints(std::move(component_update_data_),
                                      std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void PreviewsHints::ParseOptimizationFilters(
    const optimization_guide::proto::Configuration& config) {
  for (const auto& blacklist : config.optimization_blacklists()) {
    base::Optional<PreviewsType> previews_type =
        ConvertProtoOptimizationTypeToPreviewsType(
            blacklist.optimization_type());
    if (previews_type == PreviewsType::LITE_PAGE_REDIRECT &&
        previews::params::IsLitePageServerPreviewsEnabled() &&
        blacklist.has_bloom_filter()) {
      RecordOptimizationFilterStatus(
          blacklist.optimization_type(),
          optimization_guide::OptimizationFilterStatus::
              kFoundServerBlacklistConfig);
      if (lite_page_redirect_blacklist_) {
        DLOG(WARNING)
            << "Found multiple blacklist configs for LITE_PAGE_REDIRECT";
        RecordOptimizationFilterStatus(
            blacklist.optimization_type(),
            optimization_guide::OptimizationFilterStatus::
                kFailedServerBlacklistDuplicateConfig);
        continue;
      }
      const auto& bloom_filter_proto = blacklist.bloom_filter();
      DCHECK_GT(bloom_filter_proto.num_hash_functions(), 0u);
      DCHECK_GT(bloom_filter_proto.num_bits(), 0u);
      DCHECK(bloom_filter_proto.has_data());
      if (!bloom_filter_proto.has_data() ||
          bloom_filter_proto.num_bits() <= 0 ||
          bloom_filter_proto.num_bits() >
              bloom_filter_proto.data().size() * 8) {
        DLOG(ERROR) << "Bloom filter config issue";
        RecordOptimizationFilterStatus(
            blacklist.optimization_type(),
            optimization_guide::OptimizationFilterStatus::
                kFailedServerBlacklistBadConfig);
        continue;
      }
      if (static_cast<int>(bloom_filter_proto.num_bits()) >
          previews::params::
                  LitePageRedirectPreviewMaxServerBlacklistByteSize() *
              8) {
        DLOG(ERROR) << "Bloom filter data exceeds maximum size of "
                    << previews::params::
                           LitePageRedirectPreviewMaxServerBlacklistByteSize()
                    << " bytes";
        RecordOptimizationFilterStatus(
            blacklist.optimization_type(),
            optimization_guide::OptimizationFilterStatus::
                kFailedServerBlacklistTooBig);
        continue;
      }
      std::unique_ptr<optimization_guide::BloomFilter> bloom_filter =
          std::make_unique<optimization_guide::BloomFilter>(
              bloom_filter_proto.num_hash_functions(),
              bloom_filter_proto.num_bits(), bloom_filter_proto.data());
      lite_page_redirect_blacklist_ =
          std::make_unique<optimization_guide::HostFilter>(
              std::move(bloom_filter));
      RecordOptimizationFilterStatus(
          blacklist.optimization_type(),
          optimization_guide::OptimizationFilterStatus::
              kCreatedServerBlacklist);
    }
  }
}

bool PreviewsHints::IsWhitelisted(
    const GURL& url,
    PreviewsType type,
    int* out_inflation_percent,
    net::EffectiveConnectionType* out_ect_threshold,
    std::string* out_serialized_hint_version_string) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(hint_cache_);

  if (!url.has_host()) {
    return false;
  }

  if (type == previews::PreviewsType::RESOURCE_LOADING_HINTS &&
      !previews::params::IsResourceLoadingHintsEnabled()) {
    return false;
  }

  // Now check HintCache for a loaded entry with a PageHint that matches |url|
  // that whitelists the optimization.
  const optimization_guide::proto::Hint* hint =
      hint_cache_->GetHintIfLoaded(url.host());
  if (!hint) {
    // TODO(dougarnett): Add UMA histogram counts for both cases of HasHint().
    return false;
  }

  const optimization_guide::proto::PageHint* matched_page_hint =
      optimization_guide::FindPageHintForURL(url, hint);
  if (!matched_page_hint) {
    return false;
  }

  for (const auto& optimization :
       matched_page_hint->whitelisted_optimizations()) {
    // Skip over any disabled experimental optimizations.
    if (optimization_guide::IsDisabledPerOptimizationHintExperiment(
            optimization)) {
      continue;
    }
    if (!IsEnabledOptimizationType(optimization.optimization_type())) {
      continue;
    }
    // Client should use this first whitelisted optimization it has enabled.
    if (ConvertProtoOptimizationTypeToPreviewsType(
            optimization.optimization_type()) != type) {
      return false;
    }
    // |type| is the first whitelisted optimization this client supports.
    // Extract any applicable metadata for it.
    if (optimization.has_previews_metadata()) {
      *out_inflation_percent =
          optimization.previews_metadata().inflation_percent();
      *out_ect_threshold = ConvertProtoEffectiveConnectionType(
          optimization.previews_metadata().max_ect_trigger());
    } else {
      *out_inflation_percent = optimization.inflation_percent();
      if (matched_page_hint->has_max_ect_trigger()) {
        *out_ect_threshold = ConvertProtoEffectiveConnectionType(
            matched_page_hint->max_ect_trigger());
      }
    }

    *out_serialized_hint_version_string = hint->version();

    return true;
  }

  return false;
}

bool PreviewsHints::IsBlacklisted(const GURL& url, PreviewsType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url.has_host()) {
    return false;
  }

  // Check large scale blacklists received from the server.
  // (At some point, we may have blacklisting to check in HintCache as well.)
  if (type == PreviewsType::LITE_PAGE_REDIRECT) {
    // If no bloom filter blacklist is provided by the component update,
    // assume a server error and return true.
    if (!lite_page_redirect_blacklist_) {
      return true;
    }

    return lite_page_redirect_blacklist_->ContainsHostSuffix(url);
  }

  return false;
}

bool PreviewsHints::MaybeLoadOptimizationHints(
    const GURL& url,
    optimization_guide::HintLoadedCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(hint_cache_);

  if (!url.has_host()) {
    return false;
  }

  hint_cache_->LoadHint(url.host(), std::move(callback));
  return hint_cache_->HasHint(url.host());
}

bool PreviewsHints::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(hint_cache_);
  DCHECK(previews::params::IsResourceLoadingHintsEnabled());

  // First find matched page hint.
  const optimization_guide::proto::PageHint* matched_page_hint =
      optimization_guide::FindPageHintForURL(
          url, hint_cache_->GetHintIfLoaded(url.host()));
  if (!matched_page_hint) {
    return false;
  }

  // Now populate the resource patterns.
  for (const auto& optimization :
       matched_page_hint->whitelisted_optimizations()) {
    if (optimization.optimization_type() !=
        optimization_guide::proto::RESOURCE_LOADING) {
      continue;
    }

    if (optimization_guide::IsDisabledPerOptimizationHintExperiment(
            optimization)) {
      continue;
    }

    google::protobuf::RepeatedPtrField<
        optimization_guide::proto::ResourceLoadingHint>
        resource_loading_hints;
    if (optimization.has_previews_metadata()) {
      resource_loading_hints =
          optimization.previews_metadata().resource_loading_hints();
    } else {
      resource_loading_hints = optimization.resource_loading_hints();
    }

    for (const auto& resource_loading_hint : resource_loading_hints) {
      if (!resource_loading_hint.resource_pattern().empty() &&
          resource_loading_hint.loading_optimization_type() ==
              optimization_guide::proto::LOADING_BLOCK_RESOURCE) {
        out_resource_patterns_to_block->push_back(
            resource_loading_hint.resource_pattern());
      }
    }
    // Done - only use first whitelisted resource loading optimization.
    return true;
  }
  return false;
}

void PreviewsHints::LogHintCacheMatch(const GURL& url,
                                      bool is_committed,
                                      net::EffectiveConnectionType ect) const {
  DCHECK(hint_cache_);

  if (hint_cache_->HasHint(url.host())) {
    if (!is_committed) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.OptimizationGuide.HintCache.HasHint.BeforeCommit", ect,
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.OptimizationGuide.HintCache.HasHint.AtCommit", ect,
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST);
      const optimization_guide::proto::Hint* hint =
          hint_cache_->GetHintIfLoaded(url.host());
      if (hint) {
        UMA_HISTOGRAM_ENUMERATION(
            "Previews.OptimizationGuide.HintCache.HostMatch.AtCommit", ect,
            net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST);
        if (optimization_guide::FindPageHintForURL(url, hint)) {
          UMA_HISTOGRAM_ENUMERATION(
              "Previews.OptimizationGuide.HintCache.PageMatch.AtCommit", ect,
              net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST);
        }
      }
    }
  }
}

}  // namespace previews
