// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

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
    enum RoundingStrategy { UNKNOWN, CLIP_PATH, OUTLINE_PROVIDER, BITMAP_MASKING }

    static RoundedCornerDelegate getDelegate(RoundingStrategy strategy,
            RoundedCornerMaskCache maskCache, int bitmask, boolean isRtL) {
        // This switch omits the default statement to force a compile failure if a new strategy is
        // added but not handled.
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
        throw new AssertionError(String.format(
                "RoundedCornerDelegateFactory doesn't handle %s rounding strategy", strategy));
    }
}
