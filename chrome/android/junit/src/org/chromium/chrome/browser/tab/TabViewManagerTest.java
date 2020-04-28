// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for the {@link TabViewManager} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabViewManagerTest {
    @Mock
    private TabImpl mTab;
    @Mock
    private TabViewProvider mTabViewProvider0;
    @Mock
    private TabViewProvider mTabViewProvider1;
    @Mock
    private TabViewProvider mTabViewProvider2;
    @Mock
    private View mTabView0;
    @Mock
    private View mTabView1;
    @Mock
    private View mTabView2;

    private TabViewManager mTabViewManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabViewManager = new TabViewManager(mTab);
        when(mTabViewProvider0.getTabViewProviderType())
                .thenReturn(TabViewManager.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[0]);
        when(mTabViewProvider1.getTabViewProviderType())
                .thenReturn(TabViewManager.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[1]);
        when(mTabViewProvider2.getTabViewProviderType())
                .thenReturn(TabViewManager.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[2]);
        when(mTabViewProvider0.getView()).thenReturn(mTabView0);
        when(mTabViewProvider1.getView()).thenReturn(mTabView1);
        when(mTabViewProvider2.getView()).thenReturn(mTabView2);
    }

    /**
     * Verifies that the {@link TabViewProvider} with the highest priority is always
     * showing after each call to {@link TabViewManager#addTabViewProvider}.
     */
    @Test
    public void testAddTabViewProvider() {
        mTabViewManager.addTabViewProvider(mTabViewProvider1);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider1,
                mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(mTabView1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider2);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider1,
                mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(mTabView1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider0);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider0,
                mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(mTabView0);
        verifyTabViewProviderOnShownCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider0, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);
    }

    /**
     * Verifies that the {@link TabViewProvider} with the highest priority is always
     * showing after each call to {@link TabViewManager#removeTabViewProvider}.
     */
    @Test
    public void testRemoveTabViewProvider() {
        mTabViewManager.addTabViewProvider(mTabViewProvider0);
        mTabViewManager.addTabViewProvider(mTabViewProvider2);
        mTabViewManager.addTabViewProvider(mTabViewProvider1);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider0,
                mTabViewManager.getCurrentTabViewProvider());

        mTabViewManager.removeTabViewProvider(mTabViewProvider0);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider1,
                mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(mTabView1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.removeTabViewProvider(mTabViewProvider2);
        Assert.assertEquals("TabViewProvider with the highest priority should be shown", mTabViewProvider1,
                mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(mTabView1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.removeTabViewProvider(mTabViewProvider1);
        Assert.assertNull("No TabViewProvider should be shown", mTabViewManager.getCurrentTabViewProvider());
        verify(mTab).setCustomView(null);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 1);
    }

    private void verifyTabViewProviderOnShownCalled(TabViewProvider mockTabViewProvider, int numberOfCalls) {
        String description = "onShown() should have been called " + numberOfCalls + " times on TabViewProvider type "
                + mockTabViewProvider.getTabViewProviderType();
        verify(mockTabViewProvider, times(numberOfCalls).description(description)).onShown();
    }

    private void verifyTabViewProviderOnHiddenCalled(TabViewProvider mockTabViewProvider, int numberOfCalls) {
        String description = "onHidden() should have been called " + numberOfCalls + " times on TabViewProvider type "
                + mockTabViewProvider.getTabViewProviderType();
        verify(mockTabViewProvider, times(numberOfCalls).description(description)).onHidden();
    }
}
