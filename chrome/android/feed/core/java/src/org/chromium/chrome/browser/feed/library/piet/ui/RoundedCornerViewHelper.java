// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.view.View;

import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners.Corners;

import java.util.Arrays;

/** Helper class to help work with rounded corner views. */
public class RoundedCornerViewHelper {
    /**
     * Returns a float[] representing the mask for which corners should be rounded with the radius.
     */
    static float[] createRoundedCornerBitMask(float radius, int cornerField, boolean isRtL) {
        final int layoutDirection = isRtL ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR;
        return createRoundedCornerBitMask(radius, cornerField, layoutDirection);
    }

    /**
     * Returns a float[] representing the mask for which corners should be rounded with the radius.
     *
     * @param layoutDirection either 0 for {@link android.view.View#LAYOUT_DIRECTION_LTR} or 1 for
     *     {@link android.view.View#LAYOUT_DIRECTION_RTL}. If an invalid value is passed in, default
     *     to LTR.
     */
    private static float[] createRoundedCornerBitMask(
            float radius, int cornerField, int layoutDirection) {
        float[] radii = new float[8];
        // If we don't have any radius, don't bother creating the mask.
        if (radius > 0) {
            if (cornerField == Corners.CORNERS_UNSPECIFIED_VALUE) {
                Arrays.fill(radii, 0, 8, radius);
                return radii;
            }

            if (layoutDirection == View.LAYOUT_DIRECTION_RTL) {
                if ((cornerField & Corners.TOP_START_VALUE) != 0) {
                    radii[2] = radius;
                    radii[3] = radius;
                }
                if ((cornerField & Corners.TOP_END_VALUE) != 0) {
                    radii[0] = radius;
                    radii[1] = radius;
                }
                if ((cornerField & Corners.BOTTOM_END_VALUE) != 0) {
                    radii[6] = radius;
                    radii[7] = radius;
                }
                if ((cornerField & Corners.BOTTOM_START_VALUE) != 0) {
                    radii[4] = radius;
                    radii[5] = radius;
                }
            } else {
                if ((cornerField & Corners.TOP_START_VALUE) != 0) {
                    radii[0] = radius;
                    radii[1] = radius;
                }
                if ((cornerField & Corners.TOP_END_VALUE) != 0) {
                    radii[2] = radius;
                    radii[3] = radius;
                }
                if ((cornerField & Corners.BOTTOM_END_VALUE) != 0) {
                    radii[4] = radius;
                    radii[5] = radius;
                }
                if ((cornerField & Corners.BOTTOM_START_VALUE) != 0) {
                    radii[6] = radius;
                    radii[7] = radius;
                }
            }
        }

        return radii;
    }

    /**
     * Evaluates whether the rounded corners are valid--meaning one of the radius values is set, and
     * is greater than zero.
     */
    public static boolean hasValidRoundedCorners(
            RoundedCorners roundedCorners, int radiusOverride) {
        return ((roundedCorners.hasRadiusPercentageOfWidth()
                        && (roundedCorners.getRadiusPercentageOfWidth() > 0))
                || (roundedCorners.hasRadiusPercentageOfHeight()
                        && (roundedCorners.getRadiusPercentageOfHeight() > 0))
                || (roundedCorners.hasRadiusDp() && (roundedCorners.getRadiusDp() > 0))
                || (radiusOverride > 0));
    }

    /**
     * Returns the radius that was passed in, or a smaller radius if necessary.
     *
     * <p>If the current radius is bigger than the width or height, or if it has adjacent rounded
     * corners and the radius is more than half of the width or height, the radius is made smaller.
     * It shrinks on all sides, even if only one corner needs to shrink--Piet does not allow
     * different corners to have different radii.
     */
    static int adjustRadiusIfTooBig(
            int width, int height, int currentRadius, RoundedCorners roundedCorners) {
        int smallerSide = Math.min(height, width);
        currentRadius = Math.min(currentRadius, smallerSide);

        // This is expected to be by far the most common case, so check this first and return fast.
        if (allFourCornersRounded(roundedCorners.getBitmask())) {
            return Math.min(currentRadius, smallerSide / 2);
        }

        // If not all corners are rounded, it may not be necessary to truncate to half the smaller
        // side. If there are only horizontally-adjacent or only vertically-adjacent corners,
        // truncate the radius to half of that side of the view. Both need to be checked in case
        // there are 3 corners rounded, which ends up being equivalent to truncating to half the
        // smaller side.
        if (hasVerticallyAdjacentRoundedCorners(roundedCorners.getBitmask())) {
            currentRadius = Math.min(currentRadius, height / 2);
        }
        if (hasHorizontallyAdjacentRoundedCorners(roundedCorners.getBitmask())) {
            currentRadius = Math.min(currentRadius, width / 2);
        }
        return currentRadius;
    }

    static boolean allFourCornersRounded(int bitmask) {
        return bitmask == 0 || bitmask == 15;
    }

    private static boolean hasVerticallyAdjacentRoundedCorners(int bitmask) {
        return (shouldRoundCorner(Corners.TOP_START, bitmask)
                       && shouldRoundCorner(Corners.BOTTOM_START, bitmask))
                || (shouldRoundCorner(Corners.TOP_END, bitmask)
                        && shouldRoundCorner(Corners.BOTTOM_END, bitmask));
    }

    private static boolean hasHorizontallyAdjacentRoundedCorners(int bitmask) {
        return (shouldRoundCorner(Corners.TOP_START, bitmask)
                       && shouldRoundCorner(Corners.TOP_END, bitmask))
                || (shouldRoundCorner(Corners.BOTTOM_START, bitmask)
                        && shouldRoundCorner(Corners.BOTTOM_END, bitmask));
    }

    /**
     * Returns whether the corner should be rounded, based on the bitmask.
     *
     * <p>A bitmask of 0 implies that all corners should be rounded. The binary bitmask digits
     * correspond to the {@link Corners} value.
     */
    static boolean shouldRoundCorner(Corners corner, int bitmask) {
        return (bitmask == 0) || (bitmask & corner.getNumber()) != 0;
    }

    // Prevent instantiation
    private RoundedCornerViewHelper() {}
}
