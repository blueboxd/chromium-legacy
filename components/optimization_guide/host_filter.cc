// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/host_filter.h"

#include <string>

namespace optimization_guide {

// Maximum number of suffixes to check per url.
const int kMaxSuffixCount = 5;

// Realistic minimum length of a host suffix.
const int kMinHostSuffix = 6;  // eg., abc.tv

HostFilter::HostFilter(std::unique_ptr<BloomFilter> bloom_filter)
    : bloom_filter_(std::move(bloom_filter)) {
  // May be created on one thread but used on another. The first call to
  // CalledOnValidSequence() will re-bind it.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HostFilter::~HostFilter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool HostFilter::ContainsHostSuffix(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First check full host name.
  if (bloom_filter_->Contains(url.host()))
    return true;

  // Now check host suffixes from shortest to longest but skipping the
  // root domain (eg, skipping "com", "org", "in", "uk").
  std::string full_host(url.host());
  int suffix_count = 1;
  auto left_pos = full_host.find_last_of('.');  // root domain position
  while ((left_pos = full_host.find_last_of('.', left_pos - 1)) !=
             std::string::npos &&
         suffix_count < kMaxSuffixCount) {
    if (full_host.length() - left_pos > kMinHostSuffix) {
      std::string suffix = full_host.substr(left_pos + 1);
      suffix_count++;
      if (bloom_filter_->Contains(suffix))
        return true;
    }
  }
  return false;
}

}  // namespace optimization_guide
