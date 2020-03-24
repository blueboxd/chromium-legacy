// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time_to_iso8601.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/base/request_priority.h"
#include "net/http/structured_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace network {

namespace {

using TrustTokenRequestSigningHelperTest = TrustTokenRequestHelperTest;

// FakeSigner returns a successful, nonempty, but meaningless, signature over
// its given signing data. It should be used for tests involving only signing,
// not verification.
class FakeSigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  void Init(base::span<const uint8_t> key) override {}
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) override {
    return std::vector<uint8_t>{'s', 'i', 'g', 'n', 'e', 'd'};
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    NOTREACHED();
    return false;
  }
};

// IdentitySigner returns a "signature" over given signing data whose value
// equals that of the signing data. This makes verifying the signature easy:
// just check if the signature being provided equals the data it's supposed to
// be signing over.
class IdentitySigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  void Init(base::span<const uint8_t> key) override {}
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) override {
    return std::vector<uint8_t>(data.begin(), data.end());
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    return std::equal(data.begin(), data.end(), signature.begin());
  }
};

// FailingSigner always fails the Sign and Verify options.
class FailingSigner : public TrustTokenRequestSigningHelper::Signer {
 public:
  void Init(base::span<const uint8_t> key) override {}
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) override {
    return base::nullopt;
  }
  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override {
    return false;
  }
};

base::Optional<base::flat_map<std::string, net::structured_headers::Item>>
DeserializeSecSignatureHeader(base::StringPiece header) {
  base::StringPairs kvs;
  if (!base::SplitStringIntoKeyValuePairs(header, '=', ',', &kvs))
    return base::nullopt;

  base::flat_map<std::string, net::structured_headers::Item> ret;
  for (const std::pair<std::string, std::string>& kv : kvs) {
    auto maybe_item = net::structured_headers::ParseItem(kv.second);
    if (!maybe_item || !maybe_item->params.empty())
      return base::nullopt;
    ret[kv.first] = std::move(maybe_item->item);
  }

  return ret;
}

// Reconstructs |request|'s canonical request data, extracts the signature from
// |request|'s Sec-Signature header, and uses the verification algorithm
// provided by the template parameter |Signer| to check that the Sec-Signature
// header's contained signature verifies.
template <typename Signer>
void ReconstructSigningDataAndAssertSignatureVerifies(
    net::URLRequest* request,
    const TrustTokenRequestCanonicalizer& canonicalizer) {
  std::string signature_header;
  ASSERT_TRUE(request->extra_request_headers().GetHeader("Sec-Signature",
                                                         &signature_header));

  base::Optional<base::flat_map<std::string, net::structured_headers::Item>>
      maybe_map = DeserializeSecSignatureHeader(signature_header);
  ASSERT_TRUE(maybe_map);

  auto it = maybe_map->find("sig");
  ASSERT_TRUE(it != maybe_map->end());
  ASSERT_TRUE(it->second.is_byte_sequence());
  base::StringPiece signature = it->second.GetString();

  it = maybe_map->find("public-key");
  ASSERT_TRUE(it != maybe_map->end());
  ASSERT_TRUE(it->second.is_byte_sequence());
  base::StringPiece public_key = it->second.GetString();

  it = maybe_map->find("sign-request-data");
  ASSERT_TRUE(it != maybe_map->end());
  ASSERT_TRUE(it->second.is_token());
  base::StringPiece sign_request_data = it->second.GetString();

  ASSERT_THAT(sign_request_data,
              AnyOf(StrEq("include"), StrEq("headers-only")));

  base::Optional<std::vector<uint8_t>> written_reconstructed_cbor =
      canonicalizer.Canonicalize(
          request, public_key,
          sign_request_data == "include"
              ? mojom::TrustTokenSignRequestData::kInclude
              : mojom::TrustTokenSignRequestData::kHeadersOnly);
  ASSERT_TRUE(written_reconstructed_cbor);

  std::vector<uint8_t> reconstructed_signing_data(
      std::begin(
          TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator),
      std::end(TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator));
  reconstructed_signing_data.insert(reconstructed_signing_data.end(),
                                    written_reconstructed_cbor->begin(),
                                    written_reconstructed_cbor->end());

  ASSERT_TRUE(Signer().Verify(base::make_span(reconstructed_signing_data),
                              base::as_bytes(base::make_span(signature)),
                              base::as_bytes(base::make_span(public_key))));
}

// Verifies that |request| has a Sec-Signature header with a "sig" field and
// extracts the request's signature from this field.
void AssertHasSignatureAndExtract(const net::URLRequest& request,
                                  std::string* signature_out) {
  std::string signature_header;
  ASSERT_TRUE(request.extra_request_headers().GetHeader("Sec-Signature",
                                                        &signature_header));

  base::StringPairs kvs;
  base::SplitStringIntoKeyValuePairs(signature_header, '=', ',', &kvs);
  base::Optional<base::flat_map<std::string, net::structured_headers::Item>>
      maybe_map = DeserializeSecSignatureHeader(std::move(signature_header));
  ASSERT_TRUE(maybe_map);
  auto it = maybe_map->find("sig");
  ASSERT_TRUE(it != maybe_map->end());
  ASSERT_TRUE(it->second.is_byte_sequence());
  *signature_out = it->second.GetString();
}

// Assert that the given signing data is a concatenation of the domain separator
// defined in TrustTokenRequestSigningHelper (initially "Trust Token v0") and a
// valid CBOR struct, and that the struct contains a field of name |field_name|;
// extract the corresponding value.
void AssertDecodesToCborAndExtractField(base::StringPiece signing_data,
                                        base::StringPiece field_name,
                                        std::string* field_value_out) {
  base::Optional<cbor::Value> parsed = cbor::Reader::Read(base::as_bytes(
      // Skip over the "Trust Token v0" domain separator.
      base::make_span(signing_data)
          .subspan(base::size(TrustTokenRequestSigningHelper::
                                  kRequestSigningDomainSeparator))));
  ASSERT_TRUE(parsed);

  const cbor::Value::MapValue& map = parsed->GetMap();
  auto it = map.find(cbor::Value(field_name));
  ASSERT_TRUE(it != map.end());
  const cbor::Value& value = it->second;
  *field_value_out = value.is_string()
                         ? value.GetString()
                         : std::string(value.GetBytestringAsString());
}

MATCHER_P(Header, name, base::StringPrintf("The header %s is present", name)) {
  return arg.extra_request_headers().HasHeader(name);
}
MATCHER_P2(Header,
           name,
           other_matcher,
           "Evaluate the given matcher on the given header, if "
           "present.") {
  std::string header;
  if (!arg.extra_request_headers().GetHeader(name, &header))
    return false;
  return Matches(other_matcher)(header);
}

}  // namespace

TEST_F(TrustTokenRequestSigningHelperTest, WontSignIfNoRedemptionRecord) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kResourceExhausted);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
}

TEST_F(TrustTokenRequestSigningHelperTest, MergesHeaders) {
  // The signing operation should fuse and lowercase the headers from the
  // "Signed-Headers" request header and the additionalSignedHeaders Fetch
  // param.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  params.additional_headers_to_sign = std::vector<std::string>{"Sec-Time"};

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  my_request->SetExtraRequestHeaderByName(
      "Signed-Headers", "Sec-Signed-Redemption-Record", /*overwrite=*/true);

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  std::string signed_headers_header_value;
  ASSERT_TRUE(my_request->extra_request_headers().GetHeader(
      "Signed-Headers", &signed_headers_header_value));

  // Headers should have been merged and lower-cased.
  EXPECT_THAT(base::SplitString(signed_headers_header_value, ",",
                                base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL),
              UnorderedElementsAre(StrEq("sec-time"),
                                   StrEq("sec-signed-redemption-record")));
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnUnsignableHeaderNameInSignedHeadersHeader) {
  // The signing operation should fail if there's an unsignable header (as
  // specified by TrustTokenRequestSigningHelper::kSignableRequestHeaders) in
  // the "Signed-Headers" request header or the additionalSignedHeaders Fetch
  // param; this tests the former.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));

  my_request->SetExtraRequestHeaderByName(
      "Signed-Headers",
      "this header name is definitely not in"
      "TrustTokenRequestSigningHelper::kSignableRequestHeaders",
      /*overwrite=*/true);

  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInvalidArgument);
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
}

TEST_F(TrustTokenRequestSigningHelperTest,
       RejectsOnUnsignableHeaderNameInAdditionalHeadersList) {
  // The signing operation should fail if there's an unsignable header (as
  // specified by TrustTokenRequestSigningHelper::kSignableRequestHeaders) in
  // the "Signed-Headers" request header or the additionalSignedHeaders Fetch
  // param; this tests the latter.

  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  params.additional_headers_to_sign = std::vector<std::string>{
      "this header name is definitely not in "
      "TrustTokenRequestSigningHelper::kSignableRequestHeaders"};

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInvalidArgument);
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
}

class TrustTokenRequestSigningHelperTestWithMockTime
    : public TrustTokenRequestSigningHelperTest {
 public:
  TrustTokenRequestSigningHelperTestWithMockTime()
      : TrustTokenRequestSigningHelperTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TrustTokenRequestSigningHelperTestWithMockTime() override = default;
};

TEST_F(TrustTokenRequestSigningHelperTestWithMockTime, ProvidesTimeHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.should_add_timestamp = true;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FakeSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(
      *my_request,
      Header("Sec-Time", StrEq(base::TimeToISO8601(base::Time::Now()))));
}

// Test SRR attachment without request signing:
TEST_F(TrustTokenRequestSigningHelperTest,
       RedemptionRecordAttachmentWithoutSigning) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  params.should_add_timestamp = true;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kOmit;

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_body("look at me! I'm a signed redemption record");
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<IdentitySigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  ASSERT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record",
                                  StrEq(base::Base64Encode(base::as_bytes(
                                      base::make_span(my_record.body()))))));
  EXPECT_THAT(*my_request, Header("Sec-Time"));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
}

// Test a round-trip sign-and-verify with no headers.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyMinimal) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  // Giving an IdentitySigner to |helper| will mean that |helper| should provide
  // its entire signing data in the request's Sec-Signature header's "sig"
  // field. ReconstructSigningDataAndAssertSignatureVerifies then reproduces
  // this canonical data's construction and checks that the reconstructed data
  // matches what |helper| produced.
  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  auto* raw_canonicalizer = canonicalizer.get();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignatureVerifies<IdentitySigner>(
          my_request.get(), *raw_canonicalizer));
}

// Test a round-trip sign-and-verify with signed headers.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyWithHeaders) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, record);
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  auto* raw_canonicalizer = canonicalizer.get();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(url::Origin::Create(GURL("https://issuer.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignatureVerifies<IdentitySigner>(
          my_request.get(), *raw_canonicalizer));
}

// Test a round-trip sign-and-verify with signed headers when adding a timestamp
// header via |should_add_timestamp|.
TEST_F(TrustTokenRequestSigningHelperTest, SignAndVerifyTimestampHeader) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  params.additional_headers_to_sign = std::vector<std::string>{"sec-time"};
  params.should_add_timestamp = true;

  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, record);

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  auto* raw_canonicalizer = canonicalizer.get();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);
  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignatureVerifies<IdentitySigner>(
          my_request.get(), *raw_canonicalizer));

  std::string signature_string;
  ASSERT_NO_FATAL_FAILURE(
      AssertHasSignatureAndExtract(*my_request, &signature_string));
  std::string retrieved_url_spec;
  ASSERT_NO_FATAL_FAILURE(AssertDecodesToCborAndExtractField(
      signature_string, "sec-time", &retrieved_url_spec));
}

// Test a round-trip sign-and-verify additionally signing over the destination
// URL (signRequestData = "include").
TEST_F(TrustTokenRequestSigningHelperTest,
       SignAndVerifyWithHeadersAndDestinationUrl) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));
  params.sign_request_data = mojom::TrustTokenSignRequestData::kInclude;

  SignedTrustTokenRedemptionRecord record;
  record.set_body("I am a signed token redemption record");
  record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, record);
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  auto canonicalizer = std::make_unique<TrustTokenRequestCanonicalizer>();
  auto* raw_canonicalizer = canonicalizer.get();
  TrustTokenRequestSigningHelper helper(store.get(), std::move(params),
                                        std::make_unique<IdentitySigner>(),
                                        std::move(canonicalizer));

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  // In addition to testing that the signing data equals
  // ReconstructSigningDataAndAssertSignatureVerifies's reconstruction of the
  // data, explicitly check that it contains a "url" field with the right value.
  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kOk);

  ASSERT_NO_FATAL_FAILURE(
      ReconstructSigningDataAndAssertSignatureVerifies<IdentitySigner>(
          my_request.get(), *raw_canonicalizer));

  // Because we're using an IdentitySigner, |signature_string| will have value
  // equal to the base64-encoded request signing data.
  std::string signature_string;
  ASSERT_NO_FATAL_FAILURE(
      AssertHasSignatureAndExtract(*my_request, &signature_string));
  std::string retrieved_url_spec;
  ASSERT_NO_FATAL_FAILURE(AssertDecodesToCborAndExtractField(
      signature_string, "url", &retrieved_url_spec));
  ASSERT_EQ(retrieved_url_spec, GURL("https://destination.com").spec());
}

// When signing fails, the request should have an empty
// Sec-Signed-Redemption-Record header attached, and none of the other headers
// that could potentially be added during signing.
TEST_F(TrustTokenRequestSigningHelperTest, CatchesSignatureFailure) {
  std::unique_ptr<TrustTokenStore> store = TrustTokenStore::CreateInMemory();

  TrustTokenRequestSigningHelper::Params params;
  params.sign_request_data = mojom::TrustTokenSignRequestData::kHeadersOnly;
  params.issuer = url::Origin::Create(GURL("https://issuer.com"));
  params.toplevel = url::Origin::Create(GURL("https://toplevel.com"));

  SignedTrustTokenRedemptionRecord my_record;
  my_record.set_public_key("key");
  store->SetRedemptionRecord(params.issuer, params.toplevel, my_record);

  params.should_add_timestamp = true;
  params.additional_headers_to_sign =
      std::vector<std::string>{"Sec-Signed-Redemption-Record"};

  // FailingSigner will fail to sign the request, so we should see the operation
  // fail.
  TrustTokenRequestSigningHelper helper(
      store.get(), std::move(params), std::make_unique<FailingSigner>(),
      std::make_unique<TrustTokenRequestCanonicalizer>());

  auto my_request = MakeURLRequest("https://destination.com/");
  my_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com/")));
  mojom::TrustTokenOperationStatus result =
      ExecuteBeginOperationAndWaitForResult(&helper, my_request.get());

  EXPECT_EQ(result, mojom::TrustTokenOperationStatus::kInternalError);
  EXPECT_THAT(*my_request, Not(Header("Signed-Headers")));
  EXPECT_THAT(*my_request, Not(Header("Sec-Time")));
  EXPECT_THAT(*my_request, Not(Header("Sec-Signature")));
  EXPECT_THAT(*my_request, Header("Sec-Signed-Redemption-Record", IsEmpty()));
}

}  // namespace network
