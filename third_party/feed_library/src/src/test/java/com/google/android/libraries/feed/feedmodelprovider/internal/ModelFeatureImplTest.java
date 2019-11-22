// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link UpdatableModelFeature}. */
@RunWith(RobolectricTestRunner.class)
public class ModelFeatureImplTest {
    private StreamFeature streamFeature;
    @Mock
    private CursorProvider cursorProvider;
    @Mock
    private ModelCursor modelCursor;

    @Before
    public void setup() {
        initMocks(this);
        String contentId = "content-id";
        streamFeature = StreamFeature.newBuilder().setContentId(contentId).build();
        when(cursorProvider.getCursor(contentId)).thenReturn(modelCursor);
    }

    @Test
    public void testBase() {
        UpdatableModelFeature modelFeature =
                new UpdatableModelFeature(streamFeature, cursorProvider);
        assertThat(modelFeature.getStreamFeature()).isEqualTo(streamFeature);
        assertThat(modelFeature.getCursor()).isEqualTo(modelCursor);
    }
}
