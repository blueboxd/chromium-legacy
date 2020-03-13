// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentStatePagerAdapter;

import java.util.List;

/**
 * Adapter used to provide First Run pages to the FirstRunActivity ViewPager.
 */
class FirstRunPagerAdapter extends FragmentStatePagerAdapter {
    private final List<FirstRunPage> mPages;

    private boolean mStopAtTheFirstPage;

    public FirstRunPagerAdapter(FragmentManager fragmentManager, List<FirstRunPage> pages) {
        super(fragmentManager);
        assert pages != null;
        assert pages.size() > 0;
        mPages = pages;
    }

    /**
     * Controls progression beyond the first page.
     * @param stop True if no progression beyond the first page is allowed.
     */
    void setStopAtTheFirstPage(boolean stop) {
        if (stop != mStopAtTheFirstPage) {
            mStopAtTheFirstPage = stop;
            notifyDataSetChanged();
        }
    }

    @Override
    public Fragment getItem(int position) {
        assert position >= 0 && position < mPages.size();
        return mPages.get(position).instantiateFragment();
    }

    @Override
    public int getCount() {
        if (mStopAtTheFirstPage) return 1;
        return mPages.size();
    }
}
