// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.contentchanged;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests for {@link StreamContentChangedListener}.
 */
@RunWith(RobolectricTestRunner.class)
public class StreamContentChangedListenerTest {
    @Mock
    ContentChangedListener contentChangedListener;

    StreamContentChangedListener streamContentChangedListener;

    @Before
    public void setup() {
        initMocks(this);
        streamContentChangedListener = new StreamContentChangedListener();
    }

    @Test
    public void testOnContentChanged() {
        streamContentChangedListener.addContentChangedListener(contentChangedListener);

        streamContentChangedListener.onContentChanged();

        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void testRemoveContentChangedListener() {
        streamContentChangedListener.addContentChangedListener(contentChangedListener);
        streamContentChangedListener.removeContentChangedListener(contentChangedListener);

        streamContentChangedListener.onContentChanged();

        verify(contentChangedListener, never()).onContentChanged();
    }
}
