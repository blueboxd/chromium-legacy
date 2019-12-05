// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.NoContentViewHolder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link NoContentDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NoContentDriverTest {
    @Mock
    private NoContentViewHolder mNoContentViewHolder;

    private NoContentDriver mNoContentDriver;

    @Before
    public void setup() {
        initMocks(this);
        mNoContentDriver = new NoContentDriver();
    }

    @Test
    public void testBind() {
        assertThat(mNoContentDriver.isBound()).isFalse();

        mNoContentDriver.bind(mNoContentViewHolder);

        assertThat(mNoContentDriver.isBound()).isTrue();
        verify(mNoContentViewHolder).bind();
    }

    @Test
    public void testUnbind() {
        mNoContentDriver.bind(mNoContentViewHolder);
        assertThat(mNoContentDriver.isBound()).isTrue();

        mNoContentDriver.unbind();

        assertThat(mNoContentDriver.isBound()).isFalse();
        verify(mNoContentViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(mNoContentDriver.isBound()).isFalse();

        mNoContentDriver.unbind();
        verifyNoMoreInteractions(mNoContentViewHolder);
    }

    @Test
    public void testMaybeRebind() {
        mNoContentDriver.bind(mNoContentViewHolder);
        assertThat(mNoContentDriver.isBound()).isTrue();

        mNoContentDriver.maybeRebind();
        assertThat(mNoContentDriver.isBound()).isTrue();
        verify(mNoContentViewHolder, times(2)).bind();
        verify(mNoContentViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        // bind/unbind to associate the noContentViewHolder with the driver
        mNoContentDriver.bind(mNoContentViewHolder);
        mNoContentDriver.unbind();
        reset(mNoContentViewHolder);

        mNoContentDriver.maybeRebind();
        assertThat(mNoContentDriver.isBound()).isFalse();
        verify(mNoContentViewHolder, never()).bind();
        verify(mNoContentViewHolder, never()).unbind();
    }
}
