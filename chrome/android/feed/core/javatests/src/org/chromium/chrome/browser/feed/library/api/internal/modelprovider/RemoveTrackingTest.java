// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.base.Function;
import org.chromium.chrome.browser.feed.library.api.internal.common.PayloadWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.InternalProtocolBuilder;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests of the {@link RemoveTracking} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RemoveTrackingTest {
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();
    private final String mRootContentId = mIdGenerators.createRootContentId(0);

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testEmpty() {
        RemoveTracking<String> removeTracking = getRemoveTracking(
                this::simpleTransform, (contentIds) -> assertThat(contentIds).hasSize(0));
        removeTracking.triggerConsumerUpdate();
    }

    @Test
    public void testMatch() {
        RemoveTracking<String> removeTracking = getRemoveTracking(
                this::simpleTransform, (contentIds) -> assertThat(contentIds).hasSize(1));
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addFeature(mIdGenerators.createFeatureContentId(0), mRootContentId);
        List<PayloadWithId> payloads = protocolBuilder.buildAsPayloadWithId();
        for (PayloadWithId payload : payloads) {
            assertThat(payload.payload.hasStreamFeature()).isTrue();
            removeTracking.filterStreamFeature(payload.payload.getStreamFeature());
        }
        removeTracking.triggerConsumerUpdate();
    }

    @Test
    public void testNoMatch() {
        RemoveTracking<String> removeTracking = getRemoveTracking(
                this::nullTransform, (contentIds) -> assertThat(contentIds).hasSize(0));
        InternalProtocolBuilder protocolBuilder = new InternalProtocolBuilder();
        protocolBuilder.addFeature(mIdGenerators.createFeatureContentId(0), mRootContentId);
        List<PayloadWithId> payloads = protocolBuilder.buildAsPayloadWithId();
        for (PayloadWithId payload : payloads) {
            assertThat(payload.payload.hasStreamFeature()).isTrue();
            removeTracking.filterStreamFeature(payload.payload.getStreamFeature());
        }
        removeTracking.triggerConsumerUpdate();
    }

    private RemoveTracking<String> getRemoveTracking(
            Function<StreamFeature, String> transformer, Consumer<List<String>> consumer) {
        return new RemoveTracking<>(transformer, consumer);
    }

    @SuppressWarnings("unused")
    private String nullTransform(StreamFeature streamFeature) {
        return null;
    }

    private String simpleTransform(StreamFeature streamFeature) {
        return streamFeature.getContentId();
    }
}
