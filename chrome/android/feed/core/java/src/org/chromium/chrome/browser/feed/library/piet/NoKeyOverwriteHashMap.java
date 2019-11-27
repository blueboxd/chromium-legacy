// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.HashMap;

/** Map that throws if you try to insert a second key with the same value. */
public class NoKeyOverwriteHashMap<K, V> extends HashMap<K, V> {
    /** A term for the items this map contains (ex. "Style" or "Template"); used in debug logs. */
    private final String mTermForContentValue;

    private final ErrorCode mErrorCodeForDuplicate;

    NoKeyOverwriteHashMap(String termForContentValue, ErrorCode errorCodeForDuplicate) {
        this.mTermForContentValue = termForContentValue;
        this.mErrorCodeForDuplicate = errorCodeForDuplicate;
    }

    @Override
    /*@Nullable*/
    public V put(K key, V value) {
        if (containsKey(key)) {
            throw new PietFatalException(mErrorCodeForDuplicate,
                    String.format("%s key '%s' already defined", mTermForContentValue, key));
        }
        return super.put(key, value);
    }
}
