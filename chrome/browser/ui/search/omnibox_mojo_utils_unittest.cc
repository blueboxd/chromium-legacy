// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/omnibox_mojo_utils.h"

#include <string>

#include <gtest/gtest.h>
#include "base/check.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

class OmniboxMojoUtilsTest : public testing::Test {
 public:
  OmniboxMojoUtilsTest() = default;
};

// Tests that all Omnibox vector icons map to an equivalent SVG for use in the
// NTP Realbox.
TEST_F(OmniboxMojoUtilsTest, VectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(
        /*is_bookmark=*/false);
    const std::string& svg_name =
        omnibox::AutocompleteMatchVectorIconToResourceName(vector_icon);
    DCHECK(!svg_name.empty() || vector_icon.name == omnibox::kPedalIcon.name ||
           vector_icon.name == omnibox::kBlankIcon.name);
  }
}
