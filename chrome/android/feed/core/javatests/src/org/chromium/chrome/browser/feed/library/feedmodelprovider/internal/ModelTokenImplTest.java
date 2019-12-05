// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/** Tests of {@link UpdatableModelToken}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModelTokenImplTest {
    @Mock
    private TokenCompletedObserver mChangeObserver;

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    public void testFeature() {
        StreamToken token = StreamToken.newBuilder().build();
        UpdatableModelToken modelToken = new UpdatableModelToken(token, false);
        assertThat(modelToken.getStreamToken()).isEqualTo(token);
    }

    @Test
    public void testChangeObserverList() {
        StreamToken token = StreamToken.newBuilder().build();
        UpdatableModelToken modelToken = new UpdatableModelToken(token, false);
        modelToken.registerObserver(mChangeObserver);
        List<TokenCompletedObserver> observers = modelToken.getObserversToNotify();
        assertThat(observers.size()).isEqualTo(1);
        assertThat(observers.get(0)).isEqualTo(mChangeObserver);
    }
}
