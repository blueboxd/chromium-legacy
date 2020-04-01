// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitments.h"

#include "base/test/bind_test_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

mojom::TrustTokenKeyCommitmentResultPtr GetCommitmentForOrigin(
    const TrustTokenKeyCommitments& commitments,
    const url::Origin& origin) {
  mojom::TrustTokenKeyCommitmentResultPtr result;
  bool ran = false;

  commitments.Get(origin, base::BindLambdaForTesting(
                              [&](mojom::TrustTokenKeyCommitmentResultPtr ptr) {
                                result = std::move(ptr);
                                ran = true;
                              }));

  CHECK(ran);
  return result;
}

TEST(TrustTokenKeyCommitments, Empty) {
  TrustTokenKeyCommitments commitments;

  // We shouldn't find any commitments in an empty store.
  EXPECT_FALSE(GetCommitmentForOrigin(
      commitments,
      url::Origin::Create(GURL("https://suitable-origin.example"))));
}

TEST(TrustTokenKeyCommitments, CantRetrieveRecordForUnsuitableOrigin) {
  TrustTokenKeyCommitments commitments;

  // Opaque origins are insecure, and, consequently, not suitable for use a
  // Trust Tokens issuer origins; so, the |Set| call should decline to store the
  // result.
  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(url::Origin(),
                          mojom::TrustTokenKeyCommitmentResult::New());
  commitments.Set(std::move(to_set));

  // We shouldn't find any commitment corresponding to an unsuitable origin.
  EXPECT_FALSE(GetCommitmentForOrigin(commitments, url::Origin()));
}

TEST(TrustTokenKeyCommitments, CanRetrieveRecordForSuitableOrigin) {
  TrustTokenKeyCommitments commitments;

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->batch_size = mojom::TrustTokenKeyCommitmentBatchSize::New(5);

  auto suitable_origin = *SuitableTrustTokenOrigin::Create(
      GURL("https://suitable-origin.example"));

  // Opaque origins are insecure, and, consequently, not suitable for use a
  // Trust Tokens issuer origins; so, the |Set| call should decline to store the
  // result.
  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(suitable_origin.origin(), expectation.Clone());
  commitments.Set(std::move(to_set));

  // We shouldn't find any commitment corresponding to an unsuitable origin.
  auto result = GetCommitmentForOrigin(commitments, suitable_origin.origin());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.Equals(expectation));
}

TEST(TrustTokenKeyCommitments, CantRetrieveRecordForOriginNotPresent) {
  TrustTokenKeyCommitments commitments;

  auto an_origin =
      *SuitableTrustTokenOrigin::Create(GURL("https://an-origin.example"));
  auto an_expectation = mojom::TrustTokenKeyCommitmentResult::New();
  an_expectation->batch_size = mojom::TrustTokenKeyCommitmentBatchSize::New(5);

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(an_origin.origin(), an_expectation.Clone());
  commitments.Set(std::move(to_set));

  auto another_origin =
      *SuitableTrustTokenOrigin::Create(GURL("https://another-origin.example"));

  // We shouldn't find any commitment corresponding to an origin not in the map.
  EXPECT_FALSE(GetCommitmentForOrigin(commitments, another_origin.origin()));
}

TEST(TrustTokenKeyCommitments, MultipleOrigins) {
  TrustTokenKeyCommitments commitments;

  SuitableTrustTokenOrigin origins[] = {
      *SuitableTrustTokenOrigin::Create(GURL("https://an-origin.example")),
      *SuitableTrustTokenOrigin::Create(GURL("https://another-origin.example")),
  };

  mojom::TrustTokenKeyCommitmentResultPtr expectations[] = {
      mojom::TrustTokenKeyCommitmentResult::New(),
      mojom::TrustTokenKeyCommitmentResult::New(),
  };

  expectations[0]->batch_size = mojom::TrustTokenKeyCommitmentBatchSize::New(0);
  expectations[1]->batch_size = mojom::TrustTokenKeyCommitmentBatchSize::New(1);

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> to_set;
  to_set.insert_or_assign(origins[0].origin(), expectations[0].Clone());
  to_set.insert_or_assign(origins[1].origin(), expectations[1].Clone());
  commitments.Set(std::move(to_set));

  for (int i : {0, 1}) {
    auto result = GetCommitmentForOrigin(commitments, origins[i].origin());
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.Equals(expectations[i]));
  }
}

}  // namespace network
