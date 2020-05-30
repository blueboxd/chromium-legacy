// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Unit test for {@link HomepagePromoVariationManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomepagePromoVariationManagerTest {
    private static final String STUDY_ENABLED_GROUP =
            ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_ENABLED;
    private static final String STUDY_TRACKING_GROUP =
            ChromeFeatureList.HOMEPAGE_PROMO_SYNTHETIC_PROMO_SEEN_TRACKING;

    @Mock
    public HomepagePromoVariationManager.SyntheticTrialHelper mStudyHelper;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        HomepagePromoVariationManager.getInstance().setStudyHelperForTests(mStudyHelper);
    }

    @After
    public void tearDown() {
        Mockito.reset(mStudyHelper);
    }

    @Test
    @SmallTest
    public void testStudyEnabled_EnabledGroup() {
        setStudyHelper(true, true, false);

        HomepagePromoVariationManager.getInstance().tagSyntheticHomepagePromoSeenGroup();

        Mockito.verify(mStudyHelper, times(1)).isClientInIPHTrackingOnlyGroup();
        Mockito.verify(mStudyHelper, times(1)).isClientInEnabledStudyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInTrackingStudyGroup();
        Mockito.verify(mStudyHelper, never()).registerSyntheticFieldTrial(anyString());
    }

    @Test
    @SmallTest
    public void testStudyEnabled_TrackingGroup() {
        setStudyHelper(true, true, true);

        HomepagePromoVariationManager.getInstance().tagSyntheticHomepagePromoSeenGroup();

        Mockito.verify(mStudyHelper, times(1)).isClientInIPHTrackingOnlyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInEnabledStudyGroup();
        Mockito.verify(mStudyHelper, times(1)).isClientInTrackingStudyGroup();
        Mockito.verify(mStudyHelper, never()).registerSyntheticFieldTrial(anyString());
    }

    /**
     * If study disabled and client in enabled group, STUDY_ENABLED_GROUP should be tagged in
     * SyntheticFieldTrial.
     */
    @Test
    @SmallTest
    public void testStudyDisabled_EnabledGroup() {
        setStudyHelper(true, false, false);

        HomepagePromoVariationManager.getInstance().tagSyntheticHomepagePromoSeenGroup();

        Mockito.verify(mStudyHelper, times(1)).isClientInIPHTrackingOnlyGroup();
        Mockito.verify(mStudyHelper, times(1)).isClientInEnabledStudyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInTrackingStudyGroup();

        Mockito.verify(mStudyHelper, times(1)).registerSyntheticFieldTrial(STUDY_ENABLED_GROUP);
        Mockito.verify(mStudyHelper, never()).registerSyntheticFieldTrial(STUDY_TRACKING_GROUP);
    }

    /**
     * If study disabled and client in tracking-only, STUDY_TRACKING_GROUP should be tagged in
     * SyntheticFieldTrial.
     */
    @Test
    @SmallTest
    public void testStudyDisabled_TrackingGroup() {
        setStudyHelper(true, false, true);

        HomepagePromoVariationManager.getInstance().tagSyntheticHomepagePromoSeenGroup();

        Mockito.verify(mStudyHelper, times(1)).isClientInIPHTrackingOnlyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInEnabledStudyGroup();
        Mockito.verify(mStudyHelper, times(1)).isClientInTrackingStudyGroup();

        Mockito.verify(mStudyHelper, never()).registerSyntheticFieldTrial(STUDY_ENABLED_GROUP);
        Mockito.verify(mStudyHelper, times(1)).registerSyntheticFieldTrial(STUDY_TRACKING_GROUP);
    }

    /**
     * If promo has never triggered, none of the study flag should be triggered, and
     * registerSyntheticFieldTrial should not happened.
     */
    @Test
    @SmallTest
    public void testNotEverTriggered() {
        setStudyHelper(false, true, true);

        HomepagePromoVariationManager.getInstance().tagSyntheticHomepagePromoSeenGroup();

        Mockito.verify(mStudyHelper, never()).isClientInIPHTrackingOnlyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInEnabledStudyGroup();
        Mockito.verify(mStudyHelper, never()).isClientInTrackingStudyGroup();
        Mockito.verify(mStudyHelper, never()).registerSyntheticFieldTrial(any());
    }

    private void setStudyHelper(
            boolean hasEverTriggered, boolean isStudyEnabled, boolean isInTrackingGroup) {
        Mockito.doReturn(hasEverTriggered).when(mStudyHelper).hasEverTriggered();

        Mockito.doReturn(isStudyEnabled).when(mStudyHelper).isClientInEnabledStudyGroup();
        Mockito.doReturn(isStudyEnabled).when(mStudyHelper).isClientInTrackingStudyGroup();

        Mockito.doReturn(isInTrackingGroup).when(mStudyHelper).isClientInIPHTrackingOnlyGroup();
    }
}
