// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.basicstream.internal.viewholders.NoContentViewHolder;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link NoContentDriver}. */
@RunWith(RobolectricTestRunner.class)
public class NoContentDriverTest {
    @Mock
    private NoContentViewHolder noContentViewHolder;

    private NoContentDriver noContentDriver;

    @Before
    public void setup() {
        initMocks(this);
        noContentDriver = new NoContentDriver();
    }

    @Test
    public void testBind() {
        assertThat(noContentDriver.isBound()).isFalse();

        noContentDriver.bind(noContentViewHolder);

        assertThat(noContentDriver.isBound()).isTrue();
        verify(noContentViewHolder).bind();
    }

    @Test
    public void testUnbind() {
        noContentDriver.bind(noContentViewHolder);
        assertThat(noContentDriver.isBound()).isTrue();

        noContentDriver.unbind();

        assertThat(noContentDriver.isBound()).isFalse();
        verify(noContentViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(noContentDriver.isBound()).isFalse();

        noContentDriver.unbind();
        verifyNoMoreInteractions(noContentViewHolder);
    }

    @Test
    public void testMaybeRebind() {
        noContentDriver.bind(noContentViewHolder);
        assertThat(noContentDriver.isBound()).isTrue();

        noContentDriver.maybeRebind();
        assertThat(noContentDriver.isBound()).isTrue();
        verify(noContentViewHolder, times(2)).bind();
        verify(noContentViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        // bind/unbind to associate the noContentViewHolder with the driver
        noContentDriver.bind(noContentViewHolder);
        noContentDriver.unbind();
        reset(noContentViewHolder);

        noContentDriver.maybeRebind();
        assertThat(noContentDriver.isBound()).isFalse();
        verify(noContentViewHolder, never()).bind();
        verify(noContentViewHolder, never()).unbind();
    }
}
