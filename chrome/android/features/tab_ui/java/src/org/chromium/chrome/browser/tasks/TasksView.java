// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.coordinator.CoordinatorLayoutForPointer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoDescriptionView;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.content_settings.CookieControlsEnforcement;

// The view of the tasks surface.
class TasksView extends CoordinatorLayoutForPointer {
    private final Context mContext;
    private FrameLayout mBodyViewContainer;
    private FrameLayout mCarouselTabSwitcherContainer;
    private AppBarLayout mHeaderView;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private IncognitoDescriptionView mIncognitoDescriptionView;
    private View.OnClickListener mIncognitoDescriptionLearnMoreListener;
    private boolean mIncognitoCookieControlsCardIsVisible;
    private boolean mIncognitoCookieControlsToggleIsChecked;
    private OnCheckedChangeListener mIncognitoCookieControlsToggleCheckedListener;
    private @CookieControlsEnforcement int mIncognitoCookieControlsToggleEnforcement =
            CookieControlsEnforcement.NO_ENFORCEMENT;
    private View.OnClickListener mIncognitoCookieControlsIconClickListener;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        assert mSearchBoxCoordinator
                != null : "#onFinishInflate should be completed before the call to initialize.";

        mSearchBoxCoordinator.initialize(activityLifecycleDispatcher);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCarouselTabSwitcherContainer =
                (FrameLayout) findViewById(R.id.carousel_tab_switcher_container);
        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mHeaderView = (AppBarLayout) findViewById(R.id.task_surface_header);
        AppBarLayout.LayoutParams layoutParams =
                (AppBarLayout.LayoutParams) mSearchBoxCoordinator.getView().getLayoutParams();
        layoutParams.setScrollFlags(AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL);
    }

    ViewGroup getCarouselTabSwitcherContainer() {
        return mCarouselTabSwitcherContainer;
    }

    ViewGroup getBodyViewContainer() {
        return findViewById(R.id.tasks_surface_body);
    }

    /**
     * Set the visibility of the tab carousel.
     * @param isVisible Whether it's visible.
     */
    void setTabCarouselVisibility(boolean isVisible) {
        mCarouselTabSwitcherContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        findViewById(R.id.tab_switcher_title).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return The {@link SearchBoxCoordinator} representing the fake search box.
     */
    SearchBoxCoordinator getSearchBoxCoordinator() {
        return mSearchBoxCoordinator;
    }

    /**
     * Set the visibility of the Most Visited Tiles.
     */
    void setMostVisitedVisibility(int visibility) {
        findViewById(R.id.mv_tiles_container).setVisibility(visibility);
    }

    /**
     * Set the {@link android.view.View.OnClickListener} for More Tabs.
     */
    void setMoreTabsOnClickListener(@Nullable View.OnClickListener listener) {
        findViewById(R.id.more_tabs).setOnClickListener(listener);
    }

    /**
     * Set the incognito state.
     * @param isIncognito Whether it's in incognito mode.
     */
    void setIncognitoMode(boolean isIncognito) {
        Resources resources = mContext.getResources();
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(resources, isIncognito);
        setBackgroundColor(backgroundColor);
        mHeaderView.setBackgroundColor(backgroundColor);

        mSearchBoxCoordinator.setBackground(AppCompatResources.getDrawable(mContext,
                isIncognito ? R.drawable.fake_search_box_bg_incognito : R.drawable.ntp_search_box));
        int hintTextColor = isIncognito
                ? ApiCompatibilityUtils.getColor(resources, R.color.locationbar_light_hint_text)
                : ApiCompatibilityUtils.getColor(resources, R.color.locationbar_dark_hint_text);
        mSearchBoxCoordinator.setSearchBoxHintColor(hintTextColor);
    }

    /**
     * Initialize incognito description view.
     * Note that this interface is supposed to be called only once.
     */
    void initializeIncognitoDescriptionView() {
        assert mIncognitoDescriptionView == null;
        ViewStub stub = (ViewStub) findViewById(R.id.incognito_description_layout_stub);
        mIncognitoDescriptionView = (IncognitoDescriptionView) stub.inflate();
        if (mIncognitoDescriptionLearnMoreListener != null) {
            setIncognitoDescriptionLearnMoreClickListener(mIncognitoDescriptionLearnMoreListener);
        }
        setIncognitoCookieControlsCardVisibility(mIncognitoCookieControlsCardIsVisible);
        setIncognitoCookieControlsToggleChecked(mIncognitoCookieControlsToggleIsChecked);
        if (mIncognitoCookieControlsToggleCheckedListener != null) {
            setIncognitoCookieControlsToggleCheckedListener(
                    mIncognitoCookieControlsToggleCheckedListener);
        }
        setIncognitoCookieControlsToggleEnforcement(mIncognitoCookieControlsToggleEnforcement);
        if (mIncognitoCookieControlsIconClickListener != null) {
            setIncognitoCookieControlsIconClickListener(mIncognitoCookieControlsIconClickListener);
        }
    }

    /**
     * Set the visibility of the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoDescriptionVisibility(boolean isVisible) {
        mIncognitoDescriptionView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the incognito description learn more click listener.
     * @param listener The given click listener.
     */
    void setIncognitoDescriptionLearnMoreClickListener(View.OnClickListener listener) {
        mIncognitoDescriptionLearnMoreListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.findViewById(R.id.learn_more).setOnClickListener(listener);
            mIncognitoDescriptionLearnMoreListener = null;
        }
    }

    /**
     * Set the visibility of the cookie controls card on the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoCookieControlsCardVisibility(boolean isVisible) {
        mIncognitoCookieControlsCardIsVisible = isVisible;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.showCookieControlsCard(isVisible);
        }
    }

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mIncognitoCookieControlsToggleIsChecked = isChecked;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggle(isChecked);
        }
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mIncognitoCookieControlsToggleCheckedListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
            mIncognitoCookieControlsToggleCheckedListener = null;
        }
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mIncognitoCookieControlsToggleEnforcement = enforcement;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsEnforcement(enforcement);
        }
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconClickListener(OnClickListener listener) {
        mIncognitoCookieControlsIconClickListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsIconOnclickListener(listener);
            mIncognitoCookieControlsIconClickListener = null;
        }
    }
}
