// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/module_test_base.h"

namespace blink {

void ParametrizedModuleTest::SetUp() {
  if (UseTopLevelAwait()) {
    feature_list_.InitAndEnableFeature(features::kTopLevelAwait);
  } else {
    feature_list_.InitAndDisableFeature(features::kTopLevelAwait);
  }
  SetV8Flags(UseTopLevelAwait());
}

void ParametrizedModuleTest::TearDown() {
  feature_list_.Reset();
  SetV8Flags(base::FeatureList::IsEnabled(features::kTopLevelAwait));
}

void ParametrizedModuleTest::SetV8Flags(bool use_top_level_await) {
  if (use_top_level_await) {
    v8::V8::SetFlagsFromString("--harmony-top-level-await");
  } else {
    v8::V8::SetFlagsFromString("--no-harmony-top-level-await");
  }
}

}  // namespace blink
