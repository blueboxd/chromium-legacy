// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SPLIT_VIEW_H_
#define ASH_PUBLIC_CPP_SPLIT_VIEW_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class SplitViewState {
  kNoSnap,
  kLeftSnapped,
  kRightSnapped,
  kBothSnapped,
};

class ASH_PUBLIC_EXPORT SplitViewObserver {
 public:
  // Called when split view state changed from |previous_state| to |state|.
  virtual void OnSplitViewStateChanged(SplitViewState previous_state,
                                       SplitViewState state) {}

  // Called when split view divider's position has changed.
  virtual void OnSplitViewDividerPositionChanged() {}

 protected:
  SplitViewObserver() = default;
  virtual ~SplitViewObserver() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SPLIT_VIEW_H_
