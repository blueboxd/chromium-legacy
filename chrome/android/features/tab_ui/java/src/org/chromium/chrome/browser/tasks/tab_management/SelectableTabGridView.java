// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.chrome.browser.widget.selection.SelectableItemView;

/**
 * Holds the view for a selectable tab grid.
 */
public class SelectableTabGridView extends SelectableItemView<Integer> {
    public SelectableTabGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onClick() {
        super.onClick(this);
    }
}
