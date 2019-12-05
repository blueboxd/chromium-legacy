// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link UpdatableModelFeature}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModelFeatureImplTest {
    private StreamFeature mStreamFeature;
    @Mock
    private CursorProvider mCursorProvider;
    @Mock
    private ModelCursor mModelCursor;

    @Before
    public void setup() {
        initMocks(this);
        String contentId = "content-id";
        mStreamFeature = StreamFeature.newBuilder().setContentId(contentId).build();
        when(mCursorProvider.getCursor(contentId)).thenReturn(mModelCursor);
    }

    @Test
    public void testBase() {
        UpdatableModelFeature modelFeature =
                new UpdatableModelFeature(mStreamFeature, mCursorProvider);
        assertThat(modelFeature.getStreamFeature()).isEqualTo(mStreamFeature);
        assertThat(modelFeature.getCursor()).isEqualTo(mModelCursor);
    }
}
