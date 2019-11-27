// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

import java.util.ArrayList;
import java.util.List;

/** Implementation of the {@link ModelToken}. */
public final class UpdatableModelToken
        extends FeedObservable<TokenCompletedObserver> implements ModelToken {
    private final StreamToken mToken;
    private final boolean mIsSynthetic;

    public UpdatableModelToken(StreamToken token, boolean isSynthetic) {
        this.mToken = token;
        this.mIsSynthetic = isSynthetic;
    }

    @Override
    public boolean isSynthetic() {
        return mIsSynthetic;
    }

    @Override
    public StreamToken getStreamToken() {
        return mToken;
    }

    public List<TokenCompletedObserver> getObserversToNotify() {
        synchronized (mObservers) {
            return new ArrayList<>(mObservers);
        }
    }
}
