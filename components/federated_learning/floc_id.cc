// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/federated_learning/sim_hash.h"

namespace federated_learning {

namespace {

constexpr char kFlocVersion[] = "1.0.0";

constexpr size_t kNumberOfBitsInFloc = 16;
static_assert(kNumberOfBitsInFloc > 0 &&
                  kNumberOfBitsInFloc <= std::numeric_limits<uint64_t>::digits,
              "Number of bits in the floc id must be greater than 0 and no "
              "greater than 64.");

}  // namespace

// static
FlocId FlocId::CreateFromHistory(
    const std::unordered_set<std::string>& domains) {
  return FlocId(SimHashStrings(domains, kNumberOfBitsInFloc));
}

FlocId::FlocId() = default;

FlocId::FlocId(uint64_t id) : id_(id) {}

FlocId::FlocId(const FlocId& id) = default;

FlocId::~FlocId() = default;

FlocId& FlocId::operator=(const FlocId& id) = default;

FlocId& FlocId::operator=(FlocId&& id) = default;

bool FlocId::IsValid() const {
  return id_.has_value();
}

uint64_t FlocId::ToUint64() const {
  DCHECK(id_.has_value());
  return id_.value();
}

std::string FlocId::ToDebugHeaderValue() const {
  if (!id_.has_value())
    return "null";
  return ToHeaderValue();
}

std::string FlocId::ToHeaderValue() const {
  DCHECK(id_.has_value());
  return base::StrCat({base::NumberToString(id_.value()), ".", kFlocVersion});
}

}  // namespace federated_learning
