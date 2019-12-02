// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

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
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.OverviewModeObserver;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SurfaceMode;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests for {@link StartSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private PropertyModel mSecondaryTasksSurfacePropertyModel;

    @Mock
    private TabSwitcher.Controller mMainTabGridController;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private FakeboxDelegate mFakeBoxDelegate;
    @Mock
    private ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    @Mock
    private NightModeStateProvider mNightModeStateProvider;
    @Mock
    private LocationBarVoiceRecognitionHandler mLocationBarVoiceRecognitionHandler;
    @Mock
    private StartSurfaceMediator.SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    @Mock
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Captor
    private ArgumentCaptor<EmptyTabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    private ArgumentCaptor<EmptyTabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<OverviewModeObserver> mOverviewModeObserverCaptor;
    @Captor
    private ArgumentCaptor<UrlFocusChangeListener> mUrlFocusChangeListenerCaptor;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        MockitoAnnotations.initMocks(this);

        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);
        mSecondaryTasksSurfacePropertyModel = new PropertyModel(allProperties);

        doReturn(mNormalTabModel).when(mTabModelSelector).getModel(false);
        doReturn(false).when(mNormalTabModel).isIncognito();
        doReturn(true).when(mIncognitoTabModel).isIncognito();
        doReturn(mSecondaryTasksSurfaceController)
                .when(mSecondaryTasksSurfaceInitializer)
                .initialize();
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        mPropertyModel = null;
    }

    @Test
    public void showAndHideNoStartSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.NO_START_SURFACE);
        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.NO_START_SURFACE operations.
    }

    @Test
    public void showAndHideTasksOnlySurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mFakeBoxDelegate).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mFakeBoxDelegate)
                .removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.TASKS_ONLY operations.
    }

    // TODO(crbug.com/1020223): Test SurfaceMode.SINGLE_PANE and SurfaceMode.TWO_PANES modes.
    @Test
    public void hideTabCarouselWithNoTabs() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);

        doReturn(0).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
    }

    @Test
    public void hideTabCarouselWhenClosingLastTab() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));

        doReturn(1).when(mNormalTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
    }

    @Test
    public void reshowTabCarouselWhenTabClosureUndone() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);

        doReturn(1).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mock(Tab.class));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
    }

    @Test
    public void addAndRemoveTabModelObserverWithOverview() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.startedHiding();
        verify(mNormalTabModel).removeObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void addAndRemoveTabModelSelectorObserverWithOverview() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);

        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.startedHiding();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
    }

    @Test
    public void overviewModeStatesNormalModeSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.onClick(mock(View.class));
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));

        mediator.onBackPressed();
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        mediator.finishedHiding();
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoModeSinglePane() {
        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));

        mediator.setOverviewState(OverviewModeState.SHOWN_TABSWITCHER);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));

        mediator.setOverviewState(OverviewModeState.SHOWN_HOMEPAGE);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));

        mediator.hideOverview(false);
        mediator.finishedHiding();
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoModeTaskOnly() {
        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        mediator.showOverview(false);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackTasksOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeBoxDelegate)
                .getLocationBarVoiceRecognitionHandler();
        doReturn(true).when(mLocationBarVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);

        assertThat(mediator.getOverviewState(), equalTo(OverviewModeState.SHOWN_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    private StartSurfaceMediator createStartSurfaceMediator(@SurfaceMode int mode) {
        return new StartSurfaceMediator(mMainTabGridController, mTabModelSelector,
                mode == SurfaceMode.NO_START_SURFACE ? null : mPropertyModel,
                mode == SurfaceMode.SINGLE_PANE ? mFeedSurfaceCreator : null,
                mode == SurfaceMode.SINGLE_PANE ? mSecondaryTasksSurfaceInitializer : null, mode,
                mFakeBoxDelegate, mNightModeStateProvider);
    }
}
