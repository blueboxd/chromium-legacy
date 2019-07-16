// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/host_filter.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "net/nqe/effective_connection_type.h"

class GURL;

namespace optimization_guide {
struct HintsComponentInfo;
}

namespace previews {

// Holds previews hints extracted from a hint component.
class PreviewsHints {
 public:
  ~PreviewsHints();

  // Loads the component associated with the provided component info and creates
  // a PreviewsHints instance from it. If |component_update_info| is provided,
  // then the page hints from the component will be moved into it and later used
  // to update the component hints in the hint cache store. This must be called
  // using a background task runner as it requires a significant amount of
  // processing.
  static std::unique_ptr<PreviewsHints> CreateFromHintsComponent(
      const optimization_guide::HintsComponentInfo& info,
      std::unique_ptr<optimization_guide::HintUpdateData>
          component_update_data);

  // Creates a Hints instance from the provided hints configuration. This must
  // be called using a background task runner as it requires a significant
  // amount of processing.
  static std::unique_ptr<PreviewsHints> CreateFromHintsConfiguration(
      std::unique_ptr<optimization_guide::proto::Configuration> config,
      std::unique_ptr<optimization_guide::HintUpdateData>
          component_update_data);

  // Set |hint_cache_| and updates the hint cache's component data if
  // |component_update_data_| is not a nullptr. In the case where
  // |component_update_data_| is a nullptr, the callback is run synchronously;
  // otherwise, it is run asynchronously after the cache's component data update
  // completes.
  void Initialize(optimization_guide::HintCache* hint_cache,
                  base::OnceClosure callback);

  // Whether the URL is whitelisted for the given previews type. If so,
  // |out_inflation_percent|, |out_ect_threshold|,
  // |out_serialized_hints_version_string| will be populated if metadata is
  // available for them.
  bool IsWhitelisted(const GURL& url,
                     PreviewsType type,
                     int* out_inflation_percent,
                     net::EffectiveConnectionType* out_ect_threshold,
                     std::string* out_serialized_hints_version_string) const;

  // Whether the URL is blacklisted for the given previews type.
  bool IsBlacklisted(const GURL& url, PreviewsType type) const;

  // Returns whether |url| may have PageHints and triggers asynchronous load
  // of such hints are not currently available synchronously. |callback| is
  // called if any applicable hint data is loaded and available for |url|.
  bool MaybeLoadOptimizationHints(
      const GURL& url,
      optimization_guide::HintLoadedCallback callback) const;

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const;

  // Logs UMA for whether the HintCache has a matching Hint and also a matching
  // PageHint for |url|. Records the client's current |ect| as well. This is
  // useful for measuring the effectiveness of the page hints provided by Cacao.
  void LogHintCacheMatch(const GURL& url,
                         bool is_committed,
                         net::EffectiveConnectionType ect) const;

 private:
  friend class PreviewsHintsTest;

  // Constructs PreviewsHints with |component_update_data|. This
  // HintUpdateData is later moved into the HintCache during Initialize().
  PreviewsHints(std::unique_ptr<optimization_guide::HintUpdateData>
                    component_update_data);

  // Parses optimization filters from |config| and populates corresponding
  // supported blacklists in this object.
  void ParseOptimizationFilters(
      const optimization_guide::proto::Configuration& config);

  // Holds a pointer to the hint cache; the cache is owned by the optimization
  // guide, which is guaranteed to outlive PreviewsHints.
  optimization_guide::HintCache* hint_cache_;

  // HintUpdateData provided by the HintCache and populated during
  // PreviewsHints::Create(). |component_update_data_| is set during
  // construction and moved into the HintCache during Initialize().
  std::unique_ptr<optimization_guide::HintUpdateData> component_update_data_;

  // Blacklist of host suffixes for LITE_PAGE_REDIRECT Previews.
  std::unique_ptr<optimization_guide::HostFilter> lite_page_redirect_blacklist_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsHints);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_
