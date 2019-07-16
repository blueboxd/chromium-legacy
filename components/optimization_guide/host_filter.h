// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HOST_FILTER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HOST_FILTER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/bloom_filter.h"
#include "url/gurl.h"

namespace optimization_guide {

// HostFilter is a simple Host filter for keeping track of a set of strings
// that are represented by a Bloom filter.
//
// TODO(dougarnett): consider moving this and BloomFilter under
// components/blacklist/.
class HostFilter {
 public:
  explicit HostFilter(std::unique_ptr<BloomFilter> bloom_filter);

  virtual ~HostFilter();

  // Returns whether this filter contains a host suffix for the host part
  // of |url|. It will check at most 5 host suffixes and it may ignore simple
  // top level domain matches (such as "com" or "co.in").
  //
  // A host suffix is comprised of domain name level elements (vs. characters).
  // For example, "server1.www.company.co.in" has the following suffixes that
  // would be checked for membership:
  //   "server1.www.company.co.in"
  //   "www.company.co.in"
  //   "company.co.in"
  // This method will return true if any of those suffixes are present.
  //
  // Virtual for testing.
  virtual bool ContainsHostSuffix(const GURL& url) const;

 private:
  std::unique_ptr<BloomFilter> bloom_filter_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HostFilter);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HOST_FILTER_H_
