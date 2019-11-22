// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link ContentCache} class. */
@RunWith(RobolectricTestRunner.class)
public class ContentCacheTest {
    private final ContentIdGenerators idGenerators = new ContentIdGenerators();

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testBasicCaching() {
        ContentCache cache = new ContentCache();
        assertThat(cache.size()).isEqualTo(0);

        StreamPayload payload = StreamPayload.getDefaultInstance();
        String contentId = idGenerators.createFeatureContentId(1);
        cache.put(contentId, payload);
        assertThat(cache.size()).isEqualTo(1);
        assertThat(cache.get(contentId)).isEqualTo(payload);

        String missingId = idGenerators.createFeatureContentId(2);
        assertThat(cache.get(missingId)).isNull();
    }

    @Test
    public void testLifecycle() {
        ContentCache cache = new ContentCache();
        assertThat(cache.size()).isEqualTo(0);

        cache.startMutation();
        assertThat(cache.size()).isEqualTo(0);

        StreamPayload payload = StreamPayload.getDefaultInstance();
        String contentId = idGenerators.createFeatureContentId(1);
        cache.put(contentId, payload);
        assertThat(cache.size()).isEqualTo(1);
        assertThat(cache.get(contentId)).isEqualTo(payload);
    }

    @Test
    public void testStartMutation_resetsCache() {
        ContentCache cache = new ContentCache();
        cache.put(idGenerators.createFeatureContentId(1), StreamPayload.getDefaultInstance());

        cache.startMutation();
        assertThat(cache.size()).isEqualTo(0);
    }

    @Test
    public void testFinishMutation_resetsCache() {
        ContentCache cache = new ContentCache();

        cache.startMutation();
        cache.put(idGenerators.createFeatureContentId(1), StreamPayload.getDefaultInstance());
        cache.finishMutation();
        assertThat(cache.size()).isEqualTo(0);
    }

    @Test
    public void testReset() {
        ContentCache cache = new ContentCache();
        StreamPayload payload = StreamPayload.getDefaultInstance();
        int featureCount = 4;
        for (int i = 0; i < featureCount; i++) {
            String contentId = idGenerators.createFeatureContentId(i);
            cache.put(contentId, payload);
        }
        assertThat(cache.size()).isEqualTo(featureCount);
        cache.reset();
        assertThat(cache.size()).isEqualTo(0);
    }
}
