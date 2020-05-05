// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRIC_BUILDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRIC_BUILDER_H_

#include <cstdint>

#include "base/metrics/ukm_source_id.h"
#include "services/metrics/public/cpp/ukm_entry_builder_base.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

// IdentifiabilityMetricBuilder builds an identifiability metric encoded into a
// UkmEntry.
//
// This UkmEntry can be recorded via a UkmRecorder.
//
// # Encoding
//
// All identifiability metrics are represented using the tuple
//
//     < identifiable_surface_type, input, output >.
//
// A typical URL-Keyed-Metrics (UKM) entry looks like the following:
//
//     struct UkmEntry {
//       int64 source_id;
//       uint64 event_hash;
//       map<uint64,int64> metrics;
//     };
//
// (From //services/metrics/public/mojom/ukm_interface.mojom)
//
// This class encodes the former into the latter.
//
// The |source_id| is one that is known to UKM. Follow UKM guidelines for how to
// generate or determine the |source_id| corresponding to a document or URL.
//
// The |event_hash| is a digest of the UKM event name via
// |base::HashMetricName()|. For identifiability metrics, this is always
// UINT64_C(287024497009309687) which corresponds to 'Identifiability'.
//
// Metrics for *regular* UKM consist of a mapping from a metric ID to a metric
// value. The metric ID is a digest of the metric name as determined by
// base::IdentifiabilityDigestOfBytes(), similar to how an |event_hash| is
// derived from the event name.
//
// However, for identifiability metrics, the method for generating metric IDs
// is:
//
//     metrics_hash = (input << 8) | identifiable_surface_type;
//
// The |identifiable_surface_type| is an enumeration identifying the input
// identifier defined in |IdentifiableSurface::Type|.
//
// We lose the 8 MSBs of |input|. Retaining the lower bits allow us to use small
// (i.e. under 56-bits) numbers as-is without losing information.
//
// The |IdentifiableSurface| class encapsulates this translation.
//
// The |metrics| field in |UkmEntry| thus contains a mapping from the resulting
// |metric_hash| to the output of the identifiable surface encoded into 64-bits.
//
// To generate a 64-bit hash of a random binary blob, use
// |blink::IdentifiabilityDigestOfBytes()|. For numbers with fewer than 56
// significant bits, you can use the number itself as the input hash.
//
// As mentioned in |identifiability_metrics.h|, this function is **not** a
// cryptographic hash function. While it is expected to have a reasonable
// distribution for a uniform input, it should be assumed that finding
// collisions is trivial.
//
// E.g.:
//
// 1. A simple web exposed API that's represented using a |WebFeature|
//    constant. Values are defined in
//    blink/public/mojom/web_feature/web_feature.mojom.
//
//        identifiable_surface = IdentifiableSurface::FromTypeAndInput(
//            IdentifiableSurface::Type::kWebFeature,
//            blink::mojom::WebFeature::kDeviceOrientationSecureOrigin);
//        output = IdentifiabilityDigestOfBytes(result_as_binary_blob);
//
// 2. A surface that takes a non-trivial input represented as a binary blob:
//
//        identifiable_surface = IdentifiableSurface::FromTypeAndInput(
//            IdentifiableSurface::Type::kFancySurface,
//            IdentifiabilityDigestOfBytes(input_as_binary_blob));
//        output = IdentifiabilityDigestOfBytes(result_as_binary_blob);
class BLINK_COMMON_EXPORT IdentifiabilityMetricBuilder
    : public ukm::internal::UkmEntryBuilderBase {
 public:
  // Construct a metrics builder for the given |source_id|. The source must be
  // known to UKM.
  explicit IdentifiabilityMetricBuilder(base::UkmSourceId source_id);
  ~IdentifiabilityMetricBuilder() override;

  // Set the metric using a previously constructed |IdentifiableSurface|.
  IdentifiabilityMetricBuilder& Set(IdentifiableSurface surface,
                                    int64_t result);

  // Set the metric using a surface type, input and result.
  IdentifiabilityMetricBuilder& Set(IdentifiableSurface::Type surface_type,
                                    int64_t input,
                                    int64_t result);

  // Shadow the underlying Record() implementation until the upstream pipeline
  // is ready for identifiability metrics.
  // TODO(crbug.com/973801): Remove once the pipeline is ready.
  void Record(ukm::UkmRecorder* recorder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRIC_BUILDER_H_
