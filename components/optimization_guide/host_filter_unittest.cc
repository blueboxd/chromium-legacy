// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/host_filter.h"

#include "base/macros.h"
#include "components/optimization_guide/bloom_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

std::unique_ptr<BloomFilter> CreateBloomFilter() {
  std::unique_ptr<BloomFilter> filter = std::make_unique<BloomFilter>(
      7 /* num_hash_functions */, 8191 /* num_bits */);
  return filter;
}

TEST(HostFilterTest, TestContainsHostSuffix) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("fooco.co.uk");
  HostFilter host_filter(std::move(bloom_filter));
  EXPECT_TRUE(
      host_filter.ContainsHostSuffix(GURL("http://shopping.fooco.co.uk")));
  EXPECT_TRUE(host_filter.ContainsHostSuffix(
      GURL("https://shopping.fooco.co.uk/somepath")));
  EXPECT_TRUE(host_filter.ContainsHostSuffix(GURL("https://fooco.co.uk")));
  EXPECT_FALSE(host_filter.ContainsHostSuffix(GURL("https://nonfooco.co.uk")));
}

TEST(HostFilterTest, TestContainsHostSuffixMaxSuffix) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("one.two.three.four.co.uk");
  bloom_filter->Add("one.two.three.four.five.co.uk");
  HostFilter host_filter(std::move(bloom_filter));
  EXPECT_TRUE(host_filter.ContainsHostSuffix(
      GURL("http://host.one.two.three.four.co.uk")));
  EXPECT_FALSE(host_filter.ContainsHostSuffix(
      GURL("http://host.one.two.three.four.five.co.uk")));

  // Note: full host will match even if more than 5 elements.
  EXPECT_TRUE(host_filter.ContainsHostSuffix(
      GURL("http://one.two.three.four.five.co.uk")));
}

TEST(HostFilterTest, TestContainsHostSuffixMinSuffix) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("abc.tv");
  bloom_filter->Add("xy.tv");
  HostFilter host_filter(std::move(bloom_filter));
  EXPECT_TRUE(host_filter.ContainsHostSuffix(GURL("https://abc.tv")));
  EXPECT_TRUE(host_filter.ContainsHostSuffix(GURL("https://host.abc.tv")));
  EXPECT_FALSE(host_filter.ContainsHostSuffix(GURL("https://host.xy.tv")));

  // Note: full host will match even if less than min size.
  EXPECT_TRUE(host_filter.ContainsHostSuffix(GURL("https://xy.tv")));
}

}  // namespace

}  // namespace optimization_guide
