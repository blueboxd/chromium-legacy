// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.tab_ui.R;

// The view of the tasks surface.
class TasksView extends LinearLayout {
    private FrameLayout mTabSwitcherContainer;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherContainer = (FrameLayout) findViewById(R.id.tab_switcher_container);
    }

    ViewGroup getTabSwitcherContainer() {
        return mTabSwitcherContainer;
    }

    void setIsTabCarousel(boolean isTabCarousel) {
        if (isTabCarousel) {
            // TODO(crbug.com/982018): Change view according to incognito and dark mode.
            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT));
            findViewById(R.id.tab_switcher_title).setVisibility(View.VISIBLE);
            mTabSwitcherContainer.setLayoutParams(
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT));
        }
    }

    void setMoreTabsOnClicklistener(@Nullable View.OnClickListener listener) {
        findViewById(R.id.more_tabs).setOnClickListener(listener);
    }
}