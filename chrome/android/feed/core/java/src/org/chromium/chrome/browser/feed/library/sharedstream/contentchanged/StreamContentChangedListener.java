// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contentchanged;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;

import java.util.HashSet;

/**
 * {@link ContentChangedListener} used to notify any number of listeners when content is changed.
 */
public class StreamContentChangedListener implements ContentChangedListener {
    private final HashSet<ContentChangedListener> mListeners = new HashSet<>();

    public void addContentChangedListener(ContentChangedListener listener) {
        mListeners.add(listener);
    }

    public void removeContentChangedListener(ContentChangedListener listener) {
        mListeners.remove(listener);
    }

    @Override
    public void onContentChanged() {
        for (ContentChangedListener listener : mListeners) {
            listener.onContentChanged();
        }
    }
}
