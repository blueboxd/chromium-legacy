// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

namespace policy {

DlpContentRestrictionSet::DlpContentRestrictionSet() = default;

DlpContentRestrictionSet::DlpContentRestrictionSet(
    DlpContentRestriction restriction)
    : restriction_mask_(restriction) {}

DlpContentRestrictionSet::DlpContentRestrictionSet(
    const DlpContentRestrictionSet& restriction_set) = default;

DlpContentRestrictionSet& DlpContentRestrictionSet::operator=(
    const DlpContentRestrictionSet&) = default;

DlpContentRestrictionSet::~DlpContentRestrictionSet() = default;

bool DlpContentRestrictionSet::operator==(
    const DlpContentRestrictionSet& other) const {
  return restriction_mask_ == other.restriction_mask_;
}

bool DlpContentRestrictionSet::operator!=(
    const DlpContentRestrictionSet& other) const {
  return !(*this == other);
}

void DlpContentRestrictionSet::SetRestriction(
    DlpContentRestriction restriction) {
  restriction_mask_ |= restriction;
}

bool DlpContentRestrictionSet::HasRestriction(
    DlpContentRestriction restriction) const {
  return (restriction_mask_ & restriction) != 0;
}

void DlpContentRestrictionSet::UnionWith(
    const DlpContentRestrictionSet& other) {
  restriction_mask_ |= other.restriction_mask_;
}

DlpContentRestrictionSet DlpContentRestrictionSet::DifferenceWith(
    const DlpContentRestrictionSet& other) const {
  // Leave only the restrictions that are present in |this|, but not in |other|.
  return DlpContentRestrictionSet(restriction_mask_ & ~other.restriction_mask_);
}

DlpContentRestrictionSet::DlpContentRestrictionSet(uint8_t mask)
    : restriction_mask_(mask) {}

}  // namespace policy
