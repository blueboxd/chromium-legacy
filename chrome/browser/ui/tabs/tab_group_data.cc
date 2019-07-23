// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_data.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/utils/SkRandom.h"

TabGroupData::TabGroupData() : tab_count_(0) {
  static int next_placeholder_title_number = 1;
  title_ = base::ASCIIToUTF16(
      "Group " + base::NumberToString(next_placeholder_title_number));
  ++next_placeholder_title_number;

  static SkRandom rand;
  color_ = rand.nextU() | 0xff000000;
}

void TabGroupData::TabAdded() {
  ++tab_count_;
}

void TabGroupData::TabRemoved() {
  DCHECK_GT(tab_count_, 0);
  --tab_count_;
}
