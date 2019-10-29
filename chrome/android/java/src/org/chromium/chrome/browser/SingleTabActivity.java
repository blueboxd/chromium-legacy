// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;
import android.util.Pair;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Base class for task-focused activities that need to display a single tab.
 * @param <C> - type of associated Dagger component.
 *
 * Example applications that might use this Activity would be webapps and streaming media
 * activities - anything where maintaining multiple tabs is unnecessary.
 */
public abstract class SingleTabActivity<C extends ChromeActivityComponent>
        extends ChromeActivity<C> {
    private static final int PREWARM_RENDERER_DELAY_MS = 500;

    protected static final String BUNDLE_TAB_ID = "tabId";

    @Override
    protected TabModelSelector createTabModelSelector() {
        return new SingleTabModelSelector(this, this, false);
    }

    @Override
    protected Pair<? extends TabCreator, ? extends TabCreator> createTabCreators() {
        return Pair.create(createNormalTabCreator(), null);
    }

    /** Creates TabDelegates for opening new Tabs. */
    protected TabCreator createNormalTabCreator() {
        return new TabDelegate(false /* incognito */);
    }

    @Override
    public void initializeState() {
        super.initializeState();

        createAndShowTab();
    }

    protected void createAndShowTab() {
        Tab tab = createTab();
        getTabModelSelector().setTab(tab);
        tab.show(TabSelectionType.FROM_NEW);
    }

    @Override
    public SingleTabModelSelector getTabModelSelector() {
        return (SingleTabModelSelector) super.getTabModelSelector();
    }

    /**
     * Creates the {@link Tab} used by the {@link SingleTabActivity}.
     * If the {@code savedInstanceState} exists, then the user did not intentionally close the app
     * by swiping it away in the recent tasks list.  In that case, we try to restore the tab from
     * disk.
     */
    protected Tab createTab() {
        Tab tab = null;
        TabState tabState = null;
        int tabId = Tab.INVALID_TAB_ID;
        Bundle savedInstanceState = getSavedInstanceState();
        if (savedInstanceState != null) {
            tabId = savedInstanceState.getInt(BUNDLE_TAB_ID, Tab.INVALID_TAB_ID);
            if (tabId != Tab.INVALID_TAB_ID) {
                tabState = restoreTabState(savedInstanceState, tabId);
            }
        }
        boolean unfreeze = tabId != Tab.INVALID_TAB_ID && tabState != null;
        if (unfreeze) {
            tab = TabBuilder.createFromFrozenState()
                          .setId(tabId)
                          .setWindow(getWindowAndroid())
                          .setDelegateFactory(createTabDelegateFactory())
                          .setTabState(tabState)
                          .setUnfreeze(unfreeze)
                          .build();
        } else {
            tab = new TabBuilder()
                          .setWindow(getWindowAndroid())
                          .setLaunchType(TabLaunchType.FROM_CHROME_UI)
                          .setDelegateFactory(createTabDelegateFactory())
                          .setTabState(tabState)
                          .setUnfreeze(unfreeze)
                          .build();
        }
        return tab;
    }

    /**
     * @return {@link TabDelegateFactory} to be used while creating the associated {@link Tab}.
     */
    protected abstract TabDelegateFactory createTabDelegateFactory();

    /**
     * Restore {@link TabState} from a given {@link Bundle} and tabId.
     * @param saveInstanceState The saved bundle for the last recorded state.
     * @param tabId ID of the tab restored from.
     */
    protected abstract TabState restoreTabState(Bundle savedInstanceState, int tabId);

    @Override
    protected boolean handleBackPressed() {
        Tab tab = getActivityTab();
        if (tab == null) return false;

        if (exitFullscreenIfShowing()) return true;

        if (tab.canGoBack()) {
            tab.goBack();
            return true;
        }

        return false;
    }

    @Override
    public void onUpdateStateChanged() {}

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.AGGRESSIVELY_PREWARM_RENDERERS)) {
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    // If we're not still stopped, we don't need the spare WebContents.
                    if (ApplicationStatus.getStateForActivity(SingleTabActivity.this)
                            == ActivityState.STOPPED) {
                        WarmupManager.getInstance().createSpareWebContents(!WarmupManager.FOR_CCT);
                    }
                }
            }, PREWARM_RENDERER_DELAY_MS);
        }
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.AGGRESSIVELY_PREWARM_RENDERERS)) {
            if (ChromeApplication.isSevereMemorySignal(level)) {
                WarmupManager.getInstance().destroySpareWebContents();
            }
        }
    }
}
