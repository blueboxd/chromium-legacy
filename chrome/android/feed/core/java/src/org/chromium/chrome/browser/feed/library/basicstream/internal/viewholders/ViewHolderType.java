// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import android.support.annotation.IntDef;

/**
 * Constants to specify the type of ViewHolder to create in the {@link StreamRecyclerViewAdapter}.
 */
@IntDef({ViewHolderType.TYPE_HEADER, ViewHolderType.TYPE_CARD, ViewHolderType.TYPE_CONTINUATION,
        ViewHolderType.TYPE_NO_CONTENT, ViewHolderType.TYPE_ZERO_STATE})
public @interface ViewHolderType {
    int TYPE_HEADER = 0;
    int TYPE_CARD = 1;
    int TYPE_CONTINUATION = 2;
    int TYPE_NO_CONTENT = 3;
    int TYPE_ZERO_STATE = 4;
}
