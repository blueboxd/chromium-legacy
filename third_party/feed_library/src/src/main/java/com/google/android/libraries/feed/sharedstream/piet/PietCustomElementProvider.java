// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import android.content.Context;
import android.view.View;

import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.search.now.ui.piet.ElementsProto.CustomElementData;

/**
 * A Stream implementation of a {@link CustomElementProvider} which handles Stream custom elements
 * and can delegate to a host custom element adapter if needed.
 */
public class PietCustomElementProvider implements CustomElementProvider {
    private static final String TAG = "PietCustomElementPro";

    private final Context mContext;
    /*@Nullable*/ private final CustomElementProvider mHostCustomElementProvider;

    public PietCustomElementProvider(
            Context context, /*@Nullable*/ CustomElementProvider hostCustomElementProvider) {
        this.mContext = context;
        this.mHostCustomElementProvider = hostCustomElementProvider;
    }

    @Override
    public View createCustomElement(CustomElementData customElementData) {
        // We don't currently implement any custom elements yet.  Delegate to host if there is one.
        if (mHostCustomElementProvider != null) {
            return mHostCustomElementProvider.createCustomElement(customElementData);
        }

        // Just return an empty view if there is not host.
        Logger.w(TAG, "Received request for unknown custom element");
        return new View(mContext);
    }

    @Override
    public void releaseCustomView(View customElementView, CustomElementData customElementData) {
        if (mHostCustomElementProvider != null) {
            mHostCustomElementProvider.releaseCustomView(customElementView, customElementData);
            return;
        }

        Logger.w(TAG, "Received release for unknown custom element");
    }
}
