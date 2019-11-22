// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.stream;

import android.view.View;

/**
 * Class providing a default implementation for {@link Header} instances that are not dismissible.
 */
public class NonDismissibleHeader implements Header {
    private final View view;

    public NonDismissibleHeader(View view) {
        this.view = view;
    }

    @Override
    public View getView() {
        return view;
    }

    @Override
    public boolean isDismissible() {
        return false;
    }

    @Override
    public void onDismissed() {}
}
