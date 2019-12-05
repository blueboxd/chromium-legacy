// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contentchanged;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link StreamContentChangedListener}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamContentChangedListenerTest {
    @Mock
    ContentChangedListener mContentChangedListener;

    StreamContentChangedListener mStreamContentChangedListener;

    @Before
    public void setup() {
        initMocks(this);
        mStreamContentChangedListener = new StreamContentChangedListener();
    }

    @Test
    public void testOnContentChanged() {
        mStreamContentChangedListener.addContentChangedListener(mContentChangedListener);

        mStreamContentChangedListener.onContentChanged();

        verify(mContentChangedListener).onContentChanged();
    }

    @Test
    public void testRemoveContentChangedListener() {
        mStreamContentChangedListener.addContentChangedListener(mContentChangedListener);
        mStreamContentChangedListener.removeContentChangedListener(mContentChangedListener);

        mStreamContentChangedListener.onContentChanged();

        verify(mContentChangedListener, never()).onContentChanged();
    }
}
