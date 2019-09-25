// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.MostVisitedListProperties.IS_VISIBLE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link MostVisitedListMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public final class MostVisitedListMediatorUnitTest {
    private final PropertyModel mModel = new PropertyModel(MostVisitedListProperties.ALL_KEYS);

    @Mock
    private TabModel mNewTabModel;

    @Mock
    private TabModelSelector mTabModelSelector;

    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private MostVisitedListMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mMediator = new MostVisitedListMediator(mModel, mTabModelSelector);

        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
    }

    @Test
    public void incognitoTabModel_setsNotVisible() {
        mModel.set(IS_VISIBLE, true);
        doReturn(true).when(mNewTabModel).isIncognito();

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mNewTabModel, null);

        assertFalse(mModel.get(IS_VISIBLE));
    }

    @Test
    public void regularTabModel_setsVisible() {
        mModel.set(IS_VISIBLE, false);
        doReturn(false).when(mNewTabModel).isIncognito();

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(mNewTabModel, null);

        assertTrue(mModel.get(IS_VISIBLE));
    }
}
