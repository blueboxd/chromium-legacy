// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Intent;
import android.graphics.drawable.AnimationDrawable;
import android.os.Bundle;
import android.support.design.widget.CoordinatorLayout;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.SingleTabActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.touchless.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;

/**
 * An Activity used to display the Dino Run game.
 */
public class DinoActivity extends SingleTabActivity {
    private View mSplashView;

    @Override
    protected void doLayoutInflation() {
        super.doLayoutInflation();
        mSplashView = LayoutInflater.from(this).inflate(R.layout.dino_splash_view, null);
        mSplashView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    TextView dinoImage = v.findViewById(R.id.dino_splash_text_image);
                    AnimationDrawable dinoAnimation =
                            (AnimationDrawable) dinoImage.getCompoundDrawables()[1];
                    dinoAnimation.start();
                });
        ViewGroup coordinatorLayout = (ViewGroup) findViewById(R.id.coordinator);
        CoordinatorLayout.LayoutParams layoutParams =
                new CoordinatorLayout.LayoutParams(CoordinatorLayout.LayoutParams.MATCH_PARENT,
                        CoordinatorLayout.LayoutParams.MATCH_PARENT);
        mSplashView.setLayoutParams(layoutParams);
        coordinatorLayout.addView(mSplashView);
    }

    @Override
    public void finishNativeInitialization() {
        initializeCompositorContent(new LayoutManager(getCompositorViewHolder()), null /* urlBar */,
                (ViewGroup) findViewById(android.R.id.content), null /* controlContainer */);
        getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
    }

    @Override
    protected Tab createTab() {
        Tab tab = super.createTab();
        LoadUrlParams loadUrlParams = new LoadUrlParams(UrlConstants.CHROME_DINO_URL);
        loadUrlParams.setHasUserGesture(true);
        loadUrlParams.setTransitionType(PageTransition.LINK | PageTransition.FROM_API);
        tab.loadUrl(loadUrlParams);
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                UiUtils.removeViewFromParent(mSplashView);
            }
        });
        return tab;
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {}

    @Override
    public @ChromeActivity.ActivityType int getActivityType() {
        return ChromeActivity.ActivityType.DINO;
    }

    @Override
    protected void initializeToolbar() {}

    @Override
    public boolean supportsFindInPage() {
        return false;
    }

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, ChromeFullscreenManager.ControlsPosition.NONE);
    }

    @Override
    protected TabDelegateFactory createTabDelegateFactory() {
        return new NoTouchTabDelegateFactory(
                this, getFullscreenManager().getBrowserVisibilityDelegate());
    }

    @Override
    protected TabState restoreTabState(Bundle savedInstanceState, int tabId) {
        return null;
    }
}
