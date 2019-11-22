// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.basicstream.internal.viewholders.HeaderViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.SwipeNotifier;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link HeaderDriver}. */
@RunWith(RobolectricTestRunner.class)
public class HeaderDriverTest {
    @Mock
    private Header header;
    @Mock
    private HeaderViewHolder headerViewHolder;
    @Mock
    private SwipeNotifier swipeNotifier;

    private HeaderDriver headerDriver;

    @Before
    public void setup() {
        initMocks(this);
        headerDriver = new HeaderDriver(header, swipeNotifier);
    }

    @Test
    public void testBind() {
        assertThat(headerDriver.isBound()).isFalse();

        headerDriver.bind(headerViewHolder);

        assertThat(headerDriver.isBound()).isTrue();
        verify(headerViewHolder).bind(header, swipeNotifier);
    }

    @Test
    public void testUnbind() {
        headerDriver.bind(headerViewHolder);
        assertThat(headerDriver.isBound()).isTrue();

        headerDriver.unbind();

        assertThat(headerDriver.isBound()).isFalse();
        verify(headerViewHolder).unbind();
    }

    @Test
    public void testUnbind_doesNotCallUnbindIfNotBound() {
        assertThat(headerDriver.isBound()).isFalse();

        headerDriver.unbind();
        verifyNoMoreInteractions(headerViewHolder);
    }

    @Test
    public void testMaybeRebind() {
        headerDriver.bind(headerViewHolder);
        headerDriver.maybeRebind();
        verify(headerViewHolder, times(2)).bind(header, swipeNotifier);
        verify(headerViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        headerDriver.bind(headerViewHolder);
        headerDriver.unbind();
        reset(headerViewHolder);

        headerDriver.maybeRebind();
        verify(headerViewHolder, never()).bind(header, swipeNotifier);
        verify(headerViewHolder, never()).unbind();
    }

    @Test
    public void testBind_rebindToSameViewHolder_bindsOnlyOnce() {
        // Bind twice to the same viewholder.
        headerDriver.bind(headerViewHolder);
        headerDriver.bind(headerViewHolder);

        // Only binds to the viewholder once, ignoring the second bind.
        verify(headerViewHolder).bind(header, swipeNotifier);
    }

    @Test
    public void testBind_bindWhileBoundToOtherViewHolder_unbindsOldViewHolderBindsNew() {
        HeaderViewHolder initialViewHolder = mock(HeaderViewHolder.class);

        // Bind to one ViewHolder then another.
        headerDriver.bind(initialViewHolder);

        headerDriver.bind(headerViewHolder);

        verify(initialViewHolder).unbind();
        verify(headerViewHolder).bind(header, swipeNotifier);
    }
}
