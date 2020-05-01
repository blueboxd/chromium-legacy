// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manager class that controls the experiment / layout variation for the homepage promo.
 */
public class HomepagePromoVariationManager {
    @StringDef({Variations.NONE, Variations.LARGE, Variations.COMPACT, Variations.SLIM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Variations {
        String NONE = "";
        String LARGE = "Large";
        String COMPACT = "Compact";
        String SLIM = "Slim";
    }

    private static HomepagePromoVariationManager sInstance;

    /**
     * @return Singleton instance for {@link HomepagePromoVariationManager}.
     */
    public static HomepagePromoVariationManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepagePromoVariationManager();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(HomepagePromoVariationManager mock) {
        sInstance = mock;
    }

    /**
     * Get the {@link LayoutStyle} used for
     * {@link org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator}.
     */
    @LayoutStyle
    public int getLayoutVariation() {
        String variation = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.HOMEPAGE_PROMO_CARD, "promo-card-variation");

        if (variation.equals(Variations.LARGE)) {
            return LayoutStyle.LARGE;
        }
        if (variation.equals(Variations.COMPACT)) {
            return LayoutStyle.COMPACT;
        }
        if (variation.equals(Variations.SLIM)) {
            return LayoutStyle.SLIM;
        }

        return LayoutStyle.COMPACT;
    }
}
