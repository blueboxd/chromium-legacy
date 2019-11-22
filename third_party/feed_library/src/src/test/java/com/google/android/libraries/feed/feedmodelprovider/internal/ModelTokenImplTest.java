// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.List;

/** Tests of {@link UpdatableModelToken}. */
@RunWith(RobolectricTestRunner.class)
public class ModelTokenImplTest {
    @Mock
    private TokenCompletedObserver changeObserver;

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
        modelToken.registerObserver(changeObserver);
        List<TokenCompletedObserver> observers = modelToken.getObserversToNotify();
        assertThat(observers.size()).isEqualTo(1);
        assertThat(observers.get(0)).isEqualTo(changeObserver);
    }
}
