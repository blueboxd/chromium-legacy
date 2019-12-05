// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link FeatureChangeImpl} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeatureChangeImplTest {
    @Mock
    private ModelFeature mModelFeature;

    private String mModelContentId;
    private ContentIdGenerators mIdGenerators = new ContentIdGenerators();

    @Before
    public void setup() {
        initMocks(this);
        mModelContentId = mIdGenerators.createFeatureContentId(1);
        StreamFeature streamFeature =
                StreamFeature.newBuilder().setContentId(mModelContentId).build();
        when(mModelFeature.getStreamFeature()).thenReturn(streamFeature);
    }

    @Test
    public void testStreamFeatureChange() {
        FeatureChangeImpl featureChange = new FeatureChangeImpl(mModelFeature);

        assertThat(featureChange.getModelFeature()).isEqualTo(mModelFeature);
        assertThat(featureChange.getContentId()).isEqualTo(mModelContentId);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).isEmpty();
        assertThat(featureChange.getChildChanges().getRemovedChildren()).isEmpty();
        assertThat(featureChange.isFeatureChanged()).isFalse();

        featureChange.setFeatureChanged(true);
        assertThat(featureChange.isFeatureChanged()).isTrue();
    }

    @Test
    public void testAppendChild() {
        ModelChild modelChild = mock(ModelChild.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(mModelFeature);
        featureChange.getChildChangesImpl().addAppendChild(modelChild);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).hasSize(1);
        assertThat(featureChange.getChildChanges().getAppendedChildren()).contains(modelChild);
    }

    @Test
    public void testRemoveChild() {
        ModelChild modelChild = mock(ModelChild.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(mModelFeature);
        featureChange.getChildChangesImpl().removeChild(modelChild);
        assertThat(featureChange.getChildChanges().getRemovedChildren()).hasSize(1);
        assertThat(featureChange.getChildChanges().getRemovedChildren()).contains(modelChild);
    }
}
