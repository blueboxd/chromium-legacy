// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import androidx.fragment.app.Fragment;

/**
 * A base class for all fragments in the UI.
 */
public abstract class DevUiBaseFragment extends Fragment {
    /**
     * Called by {@link MainActivity} to to give the fragment an opportunity to show an error
     * message. It's up to the {@link MainActivity} to call this or not.
     * Populates and show the error message in the given {@code errorView} or NOOP if the
     * fragment has no errors to display. Creating and showing the error message may be an
     * async operation.
     */
    /* package */ void maybeShowErrorView(PersistentErrorView errorView) {
        errorView.hide();
    }
}