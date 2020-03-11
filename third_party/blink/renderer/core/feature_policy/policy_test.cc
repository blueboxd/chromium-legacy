// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/dom_document_policy.h"
#include "third_party/blink/renderer/core/feature_policy/iframe_policy.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {
constexpr char kSelfOrigin[] = "https://selforigin.com";
constexpr char kOriginA[] = "https://example.com";
constexpr char kOriginB[] = "https://example.net";
}  // namespace

using testing::UnorderedElementsAre;

class PolicyTest : public testing::Test {
 public:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    auto origin = SecurityOrigin::CreateFromString(kSelfOrigin);
    Vector<String> messages;
    auto feature_policy = FeaturePolicy::CreateFromParentPolicy(
        nullptr, ParsedFeaturePolicy(), origin->ToUrlOrigin());
    auto header = FeaturePolicyParser::ParseHeader(
        "fullscreen *; payment 'self'; midi 'none'; camera 'self' "
        "https://example.com https://example.net",
        origin.get(), &messages);
    feature_policy->SetHeaderPolicy(header);

    auto& security_context = page_holder_->GetDocument().GetSecurityContext();
    security_context.SetSecurityOriginForTesting(origin);
    security_context.SetFeaturePolicy(std::move(feature_policy));
  }

  DOMFeaturePolicy* GetPolicy() const { return policy_; }

 protected:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<Document> document_;
  Persistent<DOMFeaturePolicy> policy_;
};

class DOMDocumentPolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ =
        MakeGarbageCollected<DOMDocumentPolicy>(&page_holder_->GetDocument());
  }
};

class IFramePolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ = MakeGarbageCollected<IFramePolicy>(
        &page_holder_->GetDocument(), ParsedFeaturePolicy(),
        SecurityOrigin::CreateFromString(kSelfOrigin));
  }
};

TEST_F(DOMDocumentPolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
}

TEST_F(DOMDocumentPolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin, kOriginA, kOriginB));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre("*"));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").IsEmpty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").IsEmpty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
}

TEST_F(DOMDocumentPolicyTest, TestAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
}

TEST_F(IFramePolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
}

TEST_F(IFramePolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").IsEmpty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").IsEmpty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
}

TEST_F(IFramePolicyTest, TestAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicy) {
  ParsedFeaturePolicy container_policy = FeaturePolicyParser::ParseAttribute(
      "geolocation 'src'; payment 'none'; midi; camera 'src'",
      SecurityOrigin::CreateFromString(kSelfOrigin),
      SecurityOrigin::CreateFromString(kOriginA), nullptr);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginA));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  // "sync-xhr" is still implicitly allowed on all origins
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
}

}  // namespace blink
