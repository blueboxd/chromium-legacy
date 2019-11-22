// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.contentchanged;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;

import java.util.HashSet;

/**
 * {@link ContentChangedListener} used to notify any number of listeners when content is changed.
 */
public class StreamContentChangedListener implements ContentChangedListener {
    private final HashSet<ContentChangedListener> listeners = new HashSet<>();

    public void addContentChangedListener(ContentChangedListener listener) {
        listeners.add(listener);
    }

    public void removeContentChangedListener(ContentChangedListener listener) {
        listeners.remove(listener);
    }

    @Override
    public void onContentChanged() {
        for (ContentChangedListener listener : listeners) {
            listener.onContentChanged();
        }
    }
}
