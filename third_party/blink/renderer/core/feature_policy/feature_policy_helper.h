// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_HELPER_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using FeatureNameMap = HashMap<String, mojom::blink::FeaturePolicyFeature>;
using DocumentPolicyFeatureSet =
    HashSet<mojom::blink::DocumentPolicyFeature,
            IntHash<mojom::blink::DocumentPolicyFeature>>;

class FeatureContext;

// This method defines the feature names which will be recognized by the parser
// for the Feature-Policy HTTP header and the <iframe> "allow" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
const FeatureNameMap& GetDefaultFeatureNameMap();

// This method defines the feature names which will be recognized by the parser
// for the Document-Policy HTTP header and the <iframe> "policy" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
const DocumentPolicyFeatureSet& GetAvailableDocumentPolicyFeatures();

// Returns true if this FeaturePolicyFeature is currently disabled by an origin
// trial (it is origin trial controlled, and the origin trial is not enabled).
// The first String param should be a name of FeaturePolicyFeature.
bool DisabledByOriginTrial(const String&, FeatureContext*);

// Returns true if this DocumentPolicyFeature is currently disabled by an origin
// trial (it is origin trial controlled, and the origin trial is not enabled).
bool DisabledByOriginTrial(mojom::blink::DocumentPolicyFeature,
                           FeatureContext*);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_HELPER_H_
