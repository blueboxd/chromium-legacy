// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
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

import org.chromium.chrome.browser.feed.library.api.client.stream.Header;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.HeaderViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.SwipeNotifier;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link HeaderDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HeaderDriverTest {
    @Mock
    private Header mHeader;
    @Mock
    private HeaderViewHolder mHeaderViewHolder;
    @Mock
    private SwipeNotifier mSwipeNotifier;

    private HeaderDriver mHeaderDriver;

    @Before
    public void setup() {
        initMocks(this);
        mHeaderDriver = new HeaderDriver(mHeader, mSwipeNotifier);
    }

    @Test
    public void testBind() {
        assertThat(mHeaderDriver.isBound()).isFalse();

        mHeaderDriver.bind(mHeaderViewHolder);

        assertThat(mHeaderDriver.isBound()).isTrue();
        verify(mHeaderViewHolder).bind(mHeader, mSwipeNotifier);
    }

    @Test
    public void testUnbind() {
        mHeaderDriver.bind(mHeaderViewHolder);
        assertThat(mHeaderDriver.isBound()).isTrue();

        mHeaderDriver.unbind();

        assertThat(mHeaderDriver.isBound()).isFalse();
        verify(mHeaderViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(mHeaderDriver.isBound()).isFalse();

        mHeaderDriver.unbind();
        verifyNoMoreInteractions(mHeaderViewHolder);
    }

    @Test
    public void testMaybeRebind() {
        mHeaderDriver.bind(mHeaderViewHolder);
        mHeaderDriver.maybeRebind();
        verify(mHeaderViewHolder, times(2)).bind(mHeader, mSwipeNotifier);
        verify(mHeaderViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        mHeaderDriver.bind(mHeaderViewHolder);
        mHeaderDriver.unbind();
        reset(mHeaderViewHolder);

        mHeaderDriver.maybeRebind();
        verify(mHeaderViewHolder, never()).bind(mHeader, mSwipeNotifier);
        verify(mHeaderViewHolder, never()).unbind();
    }

    @Test
    public void testBind_rebindToSameViewHolder_bindsOnlyOnce() {
        // Bind twice to the same viewholder.
        mHeaderDriver.bind(mHeaderViewHolder);
        mHeaderDriver.bind(mHeaderViewHolder);

        // Only binds to the viewholder once, ignoring the second bind.
        verify(mHeaderViewHolder).bind(mHeader, mSwipeNotifier);
    }

    @Test
    public void testBind_bindWhileBoundToOtherViewHolder_unbindsOldViewHolderBindsNew() {
        HeaderViewHolder initialViewHolder = mock(HeaderViewHolder.class);

        // Bind to one ViewHolder then another.
        mHeaderDriver.bind(initialViewHolder);

        mHeaderDriver.bind(mHeaderViewHolder);

        verify(initialViewHolder).unbind();
        verify(mHeaderViewHolder).bind(mHeader, mSwipeNotifier);
    }
}
