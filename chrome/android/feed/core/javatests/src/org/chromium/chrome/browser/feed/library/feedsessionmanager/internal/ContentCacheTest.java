// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ContentCache} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentCacheTest {
    private final ContentIdGenerators mIdGenerators = new ContentIdGenerators();

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testBasicCaching() {
        ContentCache cache = new ContentCache();
        assertThat(cache.size()).isEqualTo(0);

        StreamPayload payload = StreamPayload.getDefaultInstance();
        String contentId = mIdGenerators.createFeatureContentId(1);
        cache.put(contentId, payload);
        assertThat(cache.size()).isEqualTo(1);
        assertThat(cache.get(contentId)).isEqualTo(payload);

        String missingId = mIdGenerators.createFeatureContentId(2);
        assertThat(cache.get(missingId)).isNull();
    }

    @Test
    public void testLifecycle() {
        ContentCache cache = new ContentCache();
        assertThat(cache.size()).isEqualTo(0);

        cache.startMutation();
        assertThat(cache.size()).isEqualTo(0);

        StreamPayload payload = StreamPayload.getDefaultInstance();
        String contentId = mIdGenerators.createFeatureContentId(1);
        cache.put(contentId, payload);
        assertThat(cache.size()).isEqualTo(1);
        assertThat(cache.get(contentId)).isEqualTo(payload);
    }

    @Test
    public void testStartMutation_resetsCache() {
        ContentCache cache = new ContentCache();
        cache.put(mIdGenerators.createFeatureContentId(1), StreamPayload.getDefaultInstance());

        cache.startMutation();
        assertThat(cache.size()).isEqualTo(0);
    }

    @Test
    public void testFinishMutation_resetsCache() {
        ContentCache cache = new ContentCache();

        cache.startMutation();
        cache.put(mIdGenerators.createFeatureContentId(1), StreamPayload.getDefaultInstance());
        cache.finishMutation();
        assertThat(cache.size()).isEqualTo(0);
    }

    @Test
    public void testReset() {
        ContentCache cache = new ContentCache();
        StreamPayload payload = StreamPayload.getDefaultInstance();
        int featureCount = 4;
        for (int i = 0; i < featureCount; i++) {
            String contentId = mIdGenerators.createFeatureContentId(i);
            cache.put(contentId, payload);
        }
        assertThat(cache.size()).isEqualTo(featureCount);
        cache.reset();
        assertThat(cache.size()).isEqualTo(0);
    }
}
