// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IPH;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_AT_LEFT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link StartSurfaceToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceToolbarMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private StartSurfaceToolbarMediator mMediator;
    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    Runnable mDismissedCallback;
    @Captor
    private ArgumentCaptor<OverviewModeObserver> mOverviewModeObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserver;
    @Captor
    private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPropertyModel = mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(INCOGNITO_SWITCHER_VISIBLE, true)
                        .with(MENU_IS_VISIBLE, true)
                        .build();
        mMediator = new StartSurfaceToolbarMediator(mPropertyModel);
        mMediator.setOverviewModeBehavior(mOverviewModeBehavior);
        verify(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
    }

    @Test
    public void showAndHide() {
        mMediator.setStartSurfaceMode(true);

        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(MENU_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(MENU_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
    }

    @Test
    public void showAndHideSetClickable() {
        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
    }

    @Test
    public void showAndHideHomePage() {
        mMediator.setTabModelSelector(mTabModelSelector);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    @Test
    public void showHomePageWithLogo() {
        verify(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
    }

    @Test
    public void enableDisableLogo() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
    }

    @Test
    public void showHomePageWithIdentityDisc() {
        mMediator.setTabModelSelector(mTabModelSelector);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        Drawable testDrawable1 = mock(Drawable.class);
        View.OnClickListener onClickListener = mock(View.OnClickListener.class);
        mMediator.showIdentityDisc(onClickListener, testDrawable1, 5);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_CLICK_HANDLER), onClickListener);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_DESCRIPTION), 5);

        Drawable testDrawable2 = mock(Drawable.class);
        mMediator.updateIdentityDiscImage(testDrawable2);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IMAGE), testDrawable2);

        mMediator.hideIdentityDisc();
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void hideIdentityDiscInIncognito() {
        mMediator.setTabModelSelector(mTabModelSelector);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserver.capture());

        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.showIdentityDisc(mock(View.OnClickListener.class), mock(Drawable.class), 0);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelectorObserver.getValue().onTabModelSelected(
                mock(TabModel.class), mock(TabModel.class));
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void dismissIPHIfIdentityDiscNotEnabled() {
        mMediator.setTabModelSelector(mTabModelSelector);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        mMediator.showIdentityDisc(mock(View.OnClickListener.class), mock(Drawable.class), 0);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        mMediator.showIPHOnIdentityDisc(0, 0, mDismissedCallback);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IPH).dismissedCallback, mDismissedCallback);

        mMediator.hideIdentityDisc();
        mMediator.showIPHOnIdentityDisc(0, 0, mDismissedCallback);
        verify(mDismissedCallback).run();
    }

    @Test
    public void showTabSwitcher() {
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.showIdentityDisc(mock(View.OnClickListener.class), mock(Drawable.class), 0);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void homePageToTabswitcher() {
        mMediator.setTabModelSelector(mTabModelSelector);

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mMediator.showIdentityDisc(mock(View.OnClickListener.class), mock(Drawable.class), 0);
        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);
    }

    @Test
    public void showTabswitcherTasksOnly() {
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);
    }

    @Test
    public void showTabswitcherOmniboxOnlyNoIncognitoTabs() {
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY, true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_LEFT), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);
    }
}
