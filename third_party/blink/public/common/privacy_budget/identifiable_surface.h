// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_

#include <stdint.h>

#include <cstddef>
#include <functional>
#include <tuple>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// An identifiable surface.
//
// This class intends to be a lightweight wrapper over a simple integer. It
// exhibits the following characteristics:
//
//   * All methods are constexpr.
//   * Immutable.
//   * Efficient enough to pass by value.
//
class IdentifiableSurface {
 public:
  // Type of identifiable surface.
  //
  // Even though the data type is uint64_t, we can only use 8 bits due to how we
  // pack the surface type and a digest of the input into a 64 bits. See
  // README.md in this directory for details on encoding.
  //
  // These values are used for aggregation across versions. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class Type : uint64_t {
    // This type is reserved for internal use and should not be used for
    // reporting any identifiability metrics.
    kReservedInternal = 0,

    // Input is a mojom::WebFeature
    kWebFeature = 1,

    // Represents a readback of a canvas. Input is the
    // CanvasRenderingContextType.
    kCanvasReadback = 2,

    // We can use values up to and including |kMax|.
    kMax = 0xff
  };

  // Construct an IdentifiableSurface based on a precalculated metric hash. Can
  // also be used as the first step in decoding an encoded metric hash.
  static constexpr IdentifiableSurface FromMetricHash(uint64_t metric_hash) {
    return IdentifiableSurface(metric_hash);
  }

  // Construct an IdentifiableSurface based on a surface type and an input hash.
  static constexpr IdentifiableSurface FromTypeAndInput(Type type,
                                                        uint64_t input) {
    return IdentifiableSurface(KeyFromSurfaceTypeAndInput(type, input));
  }

  // Returns the UKM metric hash corresponding to this IdentifiableSurface.
  constexpr uint64_t ToUkmMetricHash() const { return metric_hash_; }

  // Returns the type of this IdentifiableSurface.
  constexpr Type GetType() const {
    return std::get<0>(SurfaceTypeAndInputFromMetricKey(metric_hash_));
  }

  // Returns the input hash for this IdentifiableSurface.
  //
  // The value that's returned can be different from what's used for
  // constructing the IdentifiableSurface via FromTypeAndInput() if the input is
  // >= 2^56.
  constexpr uint64_t GetInputHash() const {
    return std::get<1>(SurfaceTypeAndInputFromMetricKey(metric_hash_));
  }

 private:
  // Returns a 64-bit metric key given an IdentifiableSurfaceType and a 64 bit
  // input digest.
  //
  // The returned key can be used as the metric hash when invoking
  // UkmEntryBuilderBase::SetMetricInternal().
  static constexpr uint64_t KeyFromSurfaceTypeAndInput(Type type,
                                                       uint64_t input) {
    uint64_t type_as_int = static_cast<uint64_t>(type);
    return type_as_int | (input << 8);
  }

  // Returns the IdentifiableSurfaceType and the input hash given a metric key.
  //
  // This is approximately the inverse of MetricKeyFromSurfaceTypeAndInput().
  // See caveat in GetInputHash() about cases where the input hash can differ
  // from that used to construct this IdentifiableSurface.
  static constexpr std::tuple<Type, uint64_t> SurfaceTypeAndInputFromMetricKey(
      uint64_t metric) {
    return std::make_tuple(static_cast<Type>(metric & 0xff), metric >> 8);
  }

 private:
  constexpr explicit IdentifiableSurface(uint64_t metric_hash)
      : metric_hash_(metric_hash) {}
  uint64_t metric_hash_;
};

constexpr bool operator<(const IdentifiableSurface& left,
                         const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() < right.ToUkmMetricHash();
}

constexpr bool operator==(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() == right.ToUkmMetricHash();
}

constexpr bool operator!=(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() != right.ToUkmMetricHash();
}

// Hash function compatible with std::hash.
struct IdentifiableSurfaceHash {
  size_t operator()(const IdentifiableSurface& s) const {
    return std::hash<uint64_t>{}(s.ToUkmMetricHash());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_
