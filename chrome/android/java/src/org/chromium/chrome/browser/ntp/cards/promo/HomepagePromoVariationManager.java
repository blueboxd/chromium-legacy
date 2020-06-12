// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

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

    // A helper class for Synthetic Field trial helper. Adding to makes test easier.
    interface SyntheticTrialHelper {
        boolean hasEverTriggered();

        void registerSyntheticFieldTrial(String groupName);

        boolean isClientInIPHTrackingOnlyGroup();

        boolean isClientInEnabledStudyGroup();

        boolean isClientInTrackingStudyGroup();
    }

    private static HomepagePromoVariationManager sInstance;

    private SyntheticTrialHelper mStudyHelper;

    /**
     * @return Singleton instance for {@link HomepagePromoVariationManager}.
     */
    public static HomepagePromoVariationManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepagePromoVariationManager();
        }
        return sInstance;
    }

    private HomepagePromoVariationManager() {
        mStudyHelper = new SyntheticTrialHelper() {
            @Override
            public boolean hasEverTriggered() {
                Tracker tracker =
                        TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());

                return tracker.hasEverTriggered(
                        FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE, false);
            }

            @Override
            public void registerSyntheticFieldTrial(String groupName) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE, groupName);
            }

            @Override
            public boolean isClientInIPHTrackingOnlyGroup() {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE, "tracking_only", false);
            }

            @Override
            public boolean isClientInEnabledStudyGroup() {
                return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_ENABLED);
            }

            @Override
            public boolean isClientInTrackingStudyGroup() {
                return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_TRACKING);
            }
        };
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

    /**
     * If the user has ever seen the homepage promo, add a synthetic tag to the user.
     */
    public void tagSyntheticHomepagePromoSeenGroup() {
        if (mStudyHelper.hasEverTriggered()) {
            boolean isTrackingOnly = mStudyHelper.isClientInIPHTrackingOnlyGroup();

            if (!isTrackingOnly && !mStudyHelper.isClientInEnabledStudyGroup()) {
                mStudyHelper.registerSyntheticFieldTrial(
                        ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_ENABLED);
            } else if (isTrackingOnly && !mStudyHelper.isClientInTrackingStudyGroup()) {
                mStudyHelper.registerSyntheticFieldTrial(
                        ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_TRACKING);
            }
        }
    }

    @VisibleForTesting
    void setStudyHelperForTests(SyntheticTrialHelper helper) {
        mStudyHelper = helper;
    }
}
