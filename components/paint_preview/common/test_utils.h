// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_

#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Allow |mojom::Persistence| to be stringified in gtest.
std::string PersistenceParamToString(
    const ::testing::TestParamInfo<paint_preview::mojom::Persistence>&
        persistence);

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_TEST_UTILS_H_
