// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.android.libraries.feed.common.feedobservable.FeedObservable;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

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
