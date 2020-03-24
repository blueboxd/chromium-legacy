// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cryptids;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

/**
 * Allows for cryptids to be displayed on the New Tab Page under certain probabilistic conditions.
 */
public class ProbabilisticCryptidRenderer {
    // Probabilities are expressed as instances per million.
    protected static final int DENOMINATOR = 1000000;

    private static final String TAG = "ProbabilisticCryptid";

    /**
     * Determines whether cryptid should be rendered on this NTP instance, based on probability
     * factors as well as variations groups.
     * @return true if the probability conditions are met and cryptid should be shown,
     *         false otherwise
     */
    public boolean shouldUseCryptidRendering() {
        // TODO: This should be disabled unless enabled by variations.
        return getRandom() < calculateProbability(getLastRenderTimestampMillis(),
                       System.currentTimeMillis(), getRenderingMoratoriumLengthMillis(),
                       getRampUpLengthMillis(), getMaxProbability());
    }

    // Protected for testing
    protected long getLastRenderTimestampMillis() {
        // TODO: Store last instance timestamp in a pref and return it.
        return 0;
    }

    protected long getRenderingMoratoriumLengthMillis() {
        // TODO: This default should be overwritable by variations.
        return 4 * 24 * 60 * 60 * 1000; // 4 days
    }

    protected long getRampUpLengthMillis() {
        // TODO: This default should be overwritable by variations.
        return 21 * 24 * 60 * 60 * 1000; // 21 days
    }

    protected int getMaxProbability() {
        // TODO: This default should be overwritable by variations.
        return 20000; // 2%
    }

    protected int getRandom() {
        return (int) (Math.random() * DENOMINATOR);
    }

    /**
     * Calculates the probability of display at a particular moment, based on various timestamp/
     * length factors. Roughly speaking, the probability starts at 0 after a rendering event,
     * then after a moratorium period will ramp up linearly until reaching a maximum probability.
     * @param lastRenderTimestamp the stored timestamp, in millis, of the last rendering event
     * @param currentTimestamp the current time, in millis
     * @param renderingMoratoriumLength the length, in millis, of the period prior to ramp-up
     *     when probability should remain at zero
     * @param rampUpLength the length, in millis, of the period over which the linear ramp-up
     *     should occur
     * @param maxProbability the highest probability value, as a fraction of |DENOMINATOR|, that
     *     should ever be returned (i.e., the value at the end of the ramp-up period)
     * @return the probability, expressed as a fraction of |DENOMINATOR|, that cryptids will be
     *     rendered
     */
    @VisibleForTesting
    static int calculateProbability(long lastRenderTimestamp, long currentTimestamp,
            long renderingMoratoriumLength, long rampUpLength, int maxProbability) {
        if (currentTimestamp < lastRenderTimestamp) {
            Log.e(TAG, "Last render timestamp is in the future");
            return 0;
        }

        long windowStartTimestamp = lastRenderTimestamp + renderingMoratoriumLength;
        long maxProbabilityStartTimestamp = windowStartTimestamp + rampUpLength;

        if (currentTimestamp < windowStartTimestamp) {
            return 0;
        } else if (currentTimestamp > maxProbabilityStartTimestamp) {
            return maxProbability;
        }

        float fractionOfRampUp = (float) (currentTimestamp - windowStartTimestamp) / rampUpLength;
        return (int) Math.round(fractionOfRampUp * maxProbability);
    }
}
