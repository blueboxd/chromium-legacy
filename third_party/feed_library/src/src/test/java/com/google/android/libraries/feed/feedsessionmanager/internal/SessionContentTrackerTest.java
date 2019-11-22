// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.common.testing.InternalProtocolBuilder;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests of the {@link SessionContentTracker} class. */
@RunWith(RobolectricTestRunner.class)
public class SessionContentTrackerTest {
    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();

    private InternalProtocolBuilder protocolBuilder;
    private SessionContentTracker sessionContentTracker;

    @Before
    public void setUp() {
        protocolBuilder = new InternalProtocolBuilder();
        sessionContentTracker = new SessionContentTracker(/* supportsClearAll= */ true);
    }

    @Test
    public void testUpdate_features() {
        int featureCnt = 3;
        addFeatures(featureCnt);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 3 features.
        assertThat(streamStructures).hasSize(3);
        sessionContentTracker.update(streamStructures);
        assertThat(sessionContentTracker.getContentIds()).hasSize(featureCnt);
        assertThat(sessionContentTracker.contains(contentIdGenerators.createFeatureContentId(1)))
                .isTrue();
    }

    @Test
    public void testUpdate_clearWithfeatures() {
        int featureCnt = 3;
        protocolBuilder.addClearOperation();
        addFeatures(featureCnt);
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 1 clear, 3 features.
        assertThat(streamStructures).hasSize(4);
        sessionContentTracker.update(streamStructures);
        assertThat(sessionContentTracker.getContentIds()).hasSize(featureCnt);
        assertThat(sessionContentTracker.contains(contentIdGenerators.createFeatureContentId(1)))
                .isTrue();
    }

    @Test
    public void testUpdate_featuresWithClear_enabled() {
        int featureCnt = 3;
        addFeatures(featureCnt);
        protocolBuilder.addClearOperation();
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 3 features, 1 clear.
        assertThat(streamStructures).hasSize(4);
        sessionContentTracker.update(streamStructures);
        assertThat(sessionContentTracker.isEmpty()).isTrue();
    }

    @Test
    public void testUpdate_featuresWithClear_disabled() {
        sessionContentTracker = new SessionContentTracker(/* supportsClearAll= */ false);

        int featureCnt = 3;
        addFeatures(featureCnt);
        protocolBuilder.addClearOperation();
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 3 features, 1 clear.
        assertThat(streamStructures).hasSize(4);
        sessionContentTracker.update(streamStructures);
        assertThat(sessionContentTracker.isEmpty()).isFalse();
    }

    @Test
    public void testUpdate_remove() {
        int featureCnt = 2;
        addFeatures(featureCnt);
        protocolBuilder.removeFeature(contentIdGenerators.createFeatureContentId(1),
                contentIdGenerators.createRootContentId(0));
        List<StreamStructure> streamStructures = protocolBuilder.buildAsStreamStructure();

        // 2 features, 1 remove.
        assertThat(streamStructures).hasSize(3);
        sessionContentTracker.update(streamStructures);
        assertThat(sessionContentTracker.getContentIds()).hasSize(1);
    }

    @Test
    public void testUpdate_requiredContent() {
        String contentId = contentIdGenerators.createFeatureContentId(1);
        protocolBuilder.addRequiredContent(contentId);
        sessionContentTracker.update(protocolBuilder.buildAsStreamStructure());
        assertThat(sessionContentTracker.getContentIds()).hasSize(1);
        assertThat(sessionContentTracker.contains(contentId)).isTrue();
    }

    private void addFeatures(int featureCnt) {
        for (int i = 0; i < featureCnt; i++) {
            protocolBuilder.addFeature(contentIdGenerators.createFeatureContentId(i + 1),
                    contentIdGenerators.createRootContentId(0));
        }
    }
}
