// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests of the {@link SessionContentTracker} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionContentTrackerTest {
    private final ContentIdGenerators mContentIdGenerators = new ContentIdGenerators();

    private InternalProtocolBuilder mProtocolBuilder;
    private SessionContentTracker mSessionContentTracker;

    @Before
    public void setUp() {
        mProtocolBuilder = new InternalProtocolBuilder();
        mSessionContentTracker = new SessionContentTracker(/* supportsClearAll= */ true);
    }

    @Test
    public void testUpdate_features() {
        int featureCnt = 3;
        addFeatures(featureCnt);
        List<StreamStructure> streamStructures = mProtocolBuilder.buildAsStreamStructure();

        // 3 features.
        assertThat(streamStructures).hasSize(3);
        mSessionContentTracker.update(streamStructures);
        assertThat(mSessionContentTracker.getContentIds()).hasSize(featureCnt);
        assertThat(mSessionContentTracker.contains(mContentIdGenerators.createFeatureContentId(1)))
                .isTrue();
    }

    @Test
    public void testUpdate_clearWithfeatures() {
        int featureCnt = 3;
        mProtocolBuilder.addClearOperation();
        addFeatures(featureCnt);
        List<StreamStructure> streamStructures = mProtocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features.
        assertThat(streamStructures).hasSize(4);
        mSessionContentTracker.update(streamStructures);
        assertThat(mSessionContentTracker.getContentIds()).hasSize(featureCnt);
        assertThat(mSessionContentTracker.contains(mContentIdGenerators.createFeatureContentId(1)))
                .isTrue();
    }

    @Test
    public void testUpdate_featuresWithClear_enabled() {
        int featureCnt = 3;
        addFeatures(featureCnt);
        mProtocolBuilder.addClearOperation();
        List<StreamStructure> streamStructures = mProtocolBuilder.buildAsStreamStructure();

        // 3 features, 1 clear.
        assertThat(streamStructures).hasSize(4);
        mSessionContentTracker.update(streamStructures);
        assertThat(mSessionContentTracker.isEmpty()).isTrue();
    }

    @Test
    public void testUpdate_featuresWithClear_disabled() {
        mSessionContentTracker = new SessionContentTracker(/* supportsClearAll= */ false);

        int featureCnt = 3;
        addFeatures(featureCnt);
        mProtocolBuilder.addClearOperation();
        List<StreamStructure> streamStructures = mProtocolBuilder.buildAsStreamStructure();

        // 3 features, 1 clear.
        assertThat(streamStructures).hasSize(4);
        mSessionContentTracker.update(streamStructures);
        assertThat(mSessionContentTracker.isEmpty()).isFalse();
    }

    @Test
    public void testUpdate_remove() {
        int featureCnt = 2;
        addFeatures(featureCnt);
        mProtocolBuilder.removeFeature(mContentIdGenerators.createFeatureContentId(1),
                mContentIdGenerators.createRootContentId(0));
        List<StreamStructure> streamStructures = mProtocolBuilder.buildAsStreamStructure();

        // 2 features, 1 remove.
        assertThat(streamStructures).hasSize(3);
        mSessionContentTracker.update(streamStructures);
        assertThat(mSessionContentTracker.getContentIds()).hasSize(1);
    }

    @Test
    public void testUpdate_requiredContent() {
        String contentId = mContentIdGenerators.createFeatureContentId(1);
        mProtocolBuilder.addRequiredContent(contentId);
        mSessionContentTracker.update(mProtocolBuilder.buildAsStreamStructure());
        assertThat(mSessionContentTracker.getContentIds()).hasSize(1);
        assertThat(mSessionContentTracker.contains(contentId)).isTrue();
    }

    private void addFeatures(int featureCnt) {
        for (int i = 0; i < featureCnt; i++) {
            mProtocolBuilder.addFeature(mContentIdGenerators.createFeatureContentId(i + 1),
                    mContentIdGenerators.createRootContentId(0));
        }
    }
}
