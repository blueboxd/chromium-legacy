// Copyright 2019 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet.ui;

/**
 * Factory of {@link RoundedCornerDelegate}.
 *
 * <p>This factory creates the delegate that corresponds with the {@link RoundingStrategy} that is
 * passed in.
 */
class RoundedCornerDelegateFactory {

  // The outline provider delegate has no state, so optimize by making a single static instance.
  private static final OutlineProviderRoundedCornerDelegate OUTLINE_DELEGATE =
      new OutlineProviderRoundedCornerDelegate();

  /** Strategies that correspond with different {@link RoundedCornerDelegate}s. */
  enum RoundingStrategy {
    UNKNOWN,
    CLIP_PATH,
    OUTLINE_PROVIDER,
    BITMAP_MASKING
  }

  static RoundedCornerDelegate getDelegate(
      RoundingStrategy strategy, RoundedCornerMaskCache maskCache, int bitmask, boolean isRtL) {
    // This switch omits the default statement to force a compile failure if a new strategy is added
    // but not handled.
    switch (strategy) {
      case CLIP_PATH:
        return new ClipPathRoundedCornerDelegate();
      case OUTLINE_PROVIDER:
        return OUTLINE_DELEGATE;
      case BITMAP_MASKING:
        return new BitmapMaskingRoundedCornerDelegate(maskCache, bitmask, isRtL);
      case UNKNOWN:
        // This should never happen, but if it does, bitmap masking works for everything.
        return new BitmapMaskingRoundedCornerDelegate(maskCache, bitmask, isRtL);
    }
    throw new AssertionError(
        String.format(
            "RoundedCornerDelegateFactory doesn't handle %s rounding strategy", strategy));
  }
}
