// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v4.view.ViewCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Condensed new tab page for touchless devices.
 *
 * Acts as Coordinator for the new tab view structure, a recyclerview, that recycles content
 * suggestions. Will own the coordinators for the other subsections:
 *    - Most Likley carousel
 *    - Recent tab button
 */
public class TouchlessNewTabPage extends BasicNativePage {
    private static final String TAG = "TouchlessNewTabPage";

    private ModalDialogManager mModalDialogManager;
    private String mTitle;
    private int mBackgroundColor;

    private TouchlessNewTabPageMediator mMediator;
    private OpenLastTabCoordinator mOpenLastTabCoordinator;

    private FrameLayout mView;
    private TouchlessNewTabPageTopLayout mRecyclerTopmostView;
    private TouchlessRecyclerView mRecyclerView;
    private Tab mTab;
    private TouchlessContextMenuManager mContextMenuManager;
    private SiteSuggestionsCoordinator mSiteSuggestionsCoordinator;

    public TouchlessNewTabPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host); // Super calls initialize at the beginning of the constructor.
    }

    @Override
    protected void initialize(ChromeActivity activity, NativePageHost nativePageHost) {
        TraceEvent.begin(TAG);

        mModalDialogManager = activity.getModalDialogManager();
        mTab = nativePageHost.getActiveTab();
        Profile profile = mTab.getProfile();

        mTitle = activity.getResources().getString(R.string.button_new_tab);
        mBackgroundColor = ApiCompatibilityUtils.getColor(
                activity.getResources(), R.color.modern_primary_color);

        mView = (FrameLayout) LayoutInflater.from(activity).inflate(
                R.layout.new_tab_page_touchless_view, null);

        mMediator = new TouchlessNewTabPageMediator(nativePageHost.getActiveTab());

        mRecyclerView = mView.findViewById(R.id.suggestions_recycler_view);
        mRecyclerTopmostView = (TouchlessNewTabPageTopLayout) LayoutInflater.from(activity).inflate(
                R.layout.new_tab_page_touchless, mRecyclerView, false);

        OpenLastTabView openLastTabButton =
                mRecyclerTopmostView.findViewById(R.id.open_last_tab_button);
        mOpenLastTabCoordinator = new OpenLastTabCoordinator(activity, profile, nativePageHost,
                openLastTabButton, mRecyclerView.getTouchlessLayoutManager());

        // TODO(dewittj): Initialize the tile suggestions coordinator here.

        initializeContentSuggestions(
                activity, nativePageHost, mRecyclerView.getTouchlessLayoutManager());

        NewTabPageUma.recordIsUserOnline();
        NewTabPageUma.recordLoadType(activity);
        TraceEvent.end(TAG);
    }

    /*
     * TODO(dewittj): This uses SuggestionsRecyclerView and NewTabPageAdapter in a legacy manner
     * that does not properly support modern MVC code architecture.
     */
    private void initializeContentSuggestions(ChromeActivity activity,
            NativePageHost nativePageHost, TouchlessLayoutManager layoutManager) {
        long constructedTimeNs = System.nanoTime();

        NewTabPageUma.trackTimeToFirstDraw(mRecyclerView, constructedTimeNs);

        Profile profile = mTab.getProfile();
        SuggestionsDependencyFactory depsFactory = SuggestionsDependencyFactory.getInstance();
        SuggestionsSource suggestionsSource = depsFactory.createSuggestionSource(profile);
        SuggestionsEventReporter eventReporter = depsFactory.createEventReporter();
        SuggestionsNavigationDelegate navigationDelegate = new SuggestionsNavigationDelegate(
                activity, profile, nativePageHost, TabModelSelector.from(mTab));
        SuggestionsUiDelegate suggestionsUiDelegate = new SuggestionsUiDelegateImpl(
                suggestionsSource, eventReporter, navigationDelegate, profile, nativePageHost,
                GlobalDiscardableReferencePool.getReferencePool(), activity.getSnackbarManager());
        suggestionsUiDelegate.addDestructionObserver(this::destroy);

        assert suggestionsUiDelegate.getSuggestionsSource() != null;

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        Runnable closeContextMenuCallback = () -> mTab.getActivity().closeContextMenu();
        mContextMenuManager = new TouchlessContextMenuManager(activity,
                activity.getModalDialogManager(), suggestionsUiDelegate.getNavigationDelegate(),
                mRecyclerView::setTouchEnabled, closeContextMenuCallback,
                NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        UiConfig uiConfig = new UiConfig(mRecyclerView);
        mRecyclerView.init(uiConfig, closeContextMenuCallback);

        // Infinite scrolling view for site suggestions.
        mSiteSuggestionsCoordinator = new SiteSuggestionsCoordinator(mRecyclerTopmostView, profile,
                navigationDelegate, mContextMenuManager, suggestionsUiDelegate.getImageFetcher(),
                layoutManager);

        NewTabPageAdapter newTabPageAdapter = new TouchlessNewTabPageAdapter(suggestionsUiDelegate,
                mRecyclerTopmostView, uiConfig,
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile),
                mContextMenuManager, mMediator.getModel(),
                mOpenLastTabCoordinator.getFocusableComponent(),
                mSiteSuggestionsCoordinator.getFocusableComponent(),
                layoutManager.createCallbackToSetViewToFocus());

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mRecyclerView, ContentSuggestionsViewBinder::bind);

        newTabPageAdapter.refreshSuggestions();
        eventReporter.onSurfaceOpened();

        // Set after the Mediator is constructed so that it has time to refresh the suggestions
        // before requesting a layout.
        mRecyclerView.setAdapter(newTabPageAdapter);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public void destroy() {
        assert !ViewCompat.isAttachedToWindow(getView())
            : "Destroy called before removed from window";

        mMediator.destroy();
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        mSiteSuggestionsCoordinator.destroy();
        mOpenLastTabCoordinator.destroy();

        super.destroy();
    }

    public void showContextMenu() {
        View focusedView = getView().findFocus();
        if (focusedView == null) return;
        ContextMenuManager.Delegate delegate =
                ContextMenuManager.getDelegateFromFocusedView(focusedView);
        if (delegate == null) return;
        mContextMenuManager.showTouchlessContextMenu(mModalDialogManager, delegate);
    }
}
