// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_METADATA_H_

#include "base/optional.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Contains metadata that could be attached to an optimization provided by the
// Optimization Guide.
class OptimizationMetadata {
 public:
  OptimizationMetadata();
  OptimizationMetadata(const OptimizationMetadata&);
  ~OptimizationMetadata();

  base::Optional<proto::PreviewsMetadata> previews_metadata() const {
    return previews_metadata_;
  }
  void set_previews_metadata(const proto::PreviewsMetadata& previews_metadata) {
    previews_metadata_ = previews_metadata;
  }

  base::Optional<proto::PerformanceHintsMetadata> performance_hints_metadata()
      const {
    return performance_hints_metadata_;
  }
  void set_performance_hints_metadata(
      const proto::PerformanceHintsMetadata& performance_hints_metadata) {
    performance_hints_metadata_ = performance_hints_metadata;
  }

  base::Optional<proto::PublicImageMetadata> public_image_metadata() const {
    return public_image_metadata_;
  }
  void set_public_image_metadata(
      const proto::PublicImageMetadata& public_image_metadata) {
    public_image_metadata_ = public_image_metadata;
  }

  base::Optional<proto::LoadingPredictorMetadata> loading_predictor_metadata()
      const {
    return loading_predictor_metadata_;
  }
  void set_loading_predictor_metadata(
      const proto::LoadingPredictorMetadata& loading_predictor_metadata) {
    loading_predictor_metadata_ = loading_predictor_metadata;
  }

 private:
  // Only applicable for NOSCRIPT and RESOURCE_LOADING optimization types.
  base::Optional<proto::PreviewsMetadata> previews_metadata_;

  // Only applicable for the PERFORMANCE_HINTS optimization type.
  base::Optional<proto::PerformanceHintsMetadata> performance_hints_metadata_;

  // Only applicable for the COMPRESS_PUBLIC_IMAGES optimization type.
  base::Optional<proto::PublicImageMetadata> public_image_metadata_;

  // Only applicable for the LOADING_PREDICTOR optimization type.
  base::Optional<proto::LoadingPredictorMetadata> loading_predictor_metadata_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_METADATA_H_
