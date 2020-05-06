// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.HomepagePromoController.HomepagePromoStateListener;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * A mediator for the {@link FeedSurfaceCoordinator} responsible for interacting with the
 * native library and handling business logic.
 */
class FeedSurfaceMediator
        implements NewTabPageLayout.ScrollDelegate, ContextMenuManager.TouchEnabledDelegate,
                   TemplateUrlServiceObserver, ListMenu.Delegate, HomepagePromoStateListener {
    private final FeedSurfaceCoordinator mCoordinator;
    private final @Nullable SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;

    private final NativePageNavigationDelegate mPageNavigationDelegate;

    private @Nullable ScrollListener mStreamScrollListener;
    private ContentChangedListener mStreamContentChangedListener;
    private SectionHeader mSectionHeader;
    private MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SignInPromo mSignInPromo;

    private boolean mFeedEnabled;
    private boolean mHasHeader;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private boolean mHasHeaderMenu;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate} that handles page
     *                               navigation.
     */
    FeedSurfaceMediator(FeedSurfaceCoordinator coordinator,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate) {
        mCoordinator = coordinator;
        mSnapScrollHelper = snapScrollHelper;
        mSigninManager = IdentityServicesProvider.get().getSigninManager();
        mPageNavigationDelegate = pageNavigationDelegate;

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mHasHeader = mCoordinator.getSectionHeaderView() != null;
        mPrefChangeRegistrar.addObserver(Pref.NTP_ARTICLES_SECTION_ENABLED, this::updateContent);
        mHasHeaderMenu = ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_HEADER_MENU);

        // Check that there is a navigation delegate when using the feed header menu.
        if (mPageNavigationDelegate == null && mHasHeaderMenu) {
            assert false : "Need navigation delegate for header menu";
        }

        initialize();
        // Create the content.
        updateContent();
    }

    /** Clears any dependencies. */
    void destroy() {
        destroyPropertiesForStream();
        mPrefChangeRegistrar.destroy();
        TemplateUrlServiceFactory.get().removeObserver(this);
    }

    private void initialize() {
        if (mSnapScrollHelper == null) return;

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mCoordinator.getView().addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mSnapScrollHelper.handleScroll();
                });
    }

    /** Update the content based on supervised user or enterprise policy. */
    private void updateContent() {
        mFeedEnabled = FeedProcessScopeFactory.isFeedProcessEnabled();
        if ((mFeedEnabled && mCoordinator.getStream() != null)
                || (!mFeedEnabled && mCoordinator.getScrollViewForPolicy() != null)) {
            return;
        }

        if (mFeedEnabled) {
            mCoordinator.createStream();
            if (mSnapScrollHelper != null) {
                mSnapScrollHelper.setView(mCoordinator.getStream().getView());
            }
            initializePropertiesForStream();
        } else {
            destroyPropertiesForStream();
            mCoordinator.createScrollViewForPolicy();
            if (mSnapScrollHelper != null) {
                mSnapScrollHelper.setView(mCoordinator.getScrollViewForPolicy());
            }
            initializePropertiesForPolicy();
        }
    }

    /**
     * Initialize properties for UI components in the {@link NewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializePropertiesForStream() {
        Stream stream = mCoordinator.getStream();

        if (mSnapScrollHelper != null) {
            mStreamScrollListener = new ScrollListener() {
                @Override
                public void onScrollStateChanged(int state) {}

                @Override
                public void onScrolled(int dx, int dy) {
                    mSnapScrollHelper.handleScroll();
                }
            };
            stream.addScrollListener(mStreamScrollListener);
        }

        mStreamContentChangedListener = () -> {
            mStreamContentChanged = true;
            if (mSnapScrollHelper != null) mSnapScrollHelper.resetSearchBoxOnScroll(true);
        };
        stream.addOnContentChangedListener(mStreamContentChangedListener);

        boolean suggestionsVisible =
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE);

        if (mHasHeader) {
            mSectionHeader = new SectionHeader(getSectionHeaderText(suggestionsVisible),
                    suggestionsVisible, this::onSectionHeaderToggled);
            mPrefChangeRegistrar.addObserver(
                    Pref.NTP_ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
            TemplateUrlServiceFactory.get().addObserver(this);
            mCoordinator.getSectionHeaderView().setHeader(mSectionHeader);

            if (mHasHeaderMenu) {
                mSectionHeader.setMenuModelList(buildMenuItems());
                mSectionHeader.setListMenuDelegate(this::onItemSelected);
            }
        }
        // Show feed if there is no header that would allow user to hide feed.
        // This is currently only relevant for the two panes start surface.
        stream.setStreamContentVisibility(mHasHeader ? mSectionHeader.isExpanded() : true);

        if (SignInPromo.shouldCreatePromo()) {
            mSignInPromo = new FeedSignInPromo(mSigninManager);
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }

        mCoordinator.updateHeaderViews(mSignInPromo != null && mSignInPromo.isVisible());

        mMemoryPressureCallback = pressure -> mCoordinator.getStream().trim();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);
    }

    /** Clear any dependencies related to the {@link Stream}. */
    private void destroyPropertiesForStream() {
        Stream stream = mCoordinator.getStream();
        if (stream == null) return;

        if (mStreamScrollListener != null) {
            stream.removeScrollListener(mStreamScrollListener);
            mStreamScrollListener = null;
        }

        stream.removeOnContentChangedListener(mStreamContentChangedListener);
        mStreamContentChangedListener = null;

        MemoryPressureListener.removeCallback(mMemoryPressureCallback);
        mMemoryPressureCallback = null;

        if (mSignInPromo != null) {
            mSignInPromo.destroy();
            mSignInPromo = null;
        }

        mPrefChangeRegistrar.removeObserver(Pref.NTP_ARTICLES_LIST_VISIBLE);
        TemplateUrlServiceFactory.get().removeObserver(this);
    }

    /**
     * Initialize properties for the scroll view shown under supervised user or enterprise policy.
     */
    private void initializePropertiesForPolicy() {
        ScrollView view = mCoordinator.getScrollViewForPolicy();
        if (mSnapScrollHelper != null) {
            view.getViewTreeObserver().addOnScrollChangedListener(mSnapScrollHelper::handleScroll);
        }
    }

    /** Update whether the section header should be expanded and its text contents. */
    private void updateSectionHeader() {
        mSectionHeader.setHeaderText(getSectionHeaderText(mSectionHeader.isExpanded()));
        if (mHasHeaderMenu) {
            mSectionHeader.setMenuModelList(buildMenuItems());
        }

        boolean suggestionsVisible =
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE);
        if (mSectionHeader.isExpanded() != suggestionsVisible) mSectionHeader.toggleHeader();
        if (mSignInPromo != null) {
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (suggestionsVisible) mCoordinator.getStreamLifecycleManager().activate();
        mStreamContentChanged = true;
        mCoordinator.getSectionHeaderView().updateVisuals();
    }

    /**
     * Callback on section header toggled. This will update the visibility of the Feed and the
     * expand icon on the section header view.
     */
    private void onSectionHeaderToggled() {
        PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_LIST_VISIBLE, mSectionHeader.isExpanded());
        mCoordinator.getStream().setStreamContentVisibility(mSectionHeader.isExpanded());
        // TODO(huayinz): Update the section header view through a ModelChangeProcessor.
        mCoordinator.getSectionHeaderView().updateVisuals();
    }

    /** Returns the section header text based on the selected default search engine */
    private String getSectionHeaderText(boolean isExpanded) {
        Resources res = mCoordinator.getSectionHeaderView().getResources();
        final int sectionHeaderStringId;
        if (mHasHeaderMenu) {
            sectionHeaderStringId =
                    isExpanded ? R.string.ntp_discover_on : R.string.ntp_discover_off;
        } else {
            sectionHeaderStringId = TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()
                    ? R.string.ntp_article_suggestions_section_header
                    : R.string.ntp_article_suggestions_section_header_branded;
        }
        return res.getString(sectionHeaderStringId);
    }

    private ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        int icon_id = 0;
        itemList.add(buildMenuListItem(
                R.string.ntp_manage_my_activity, R.id.ntp_feed_header_menu_item_activity, icon_id));
        itemList.add(buildMenuListItem(
                R.string.ntp_manage_interests, R.id.ntp_feed_header_menu_item_interest, icon_id));
        itemList.add(buildMenuListItem(
                R.string.learn_more, R.id.ntp_feed_header_menu_item_learn, icon_id));
        if (mSectionHeader.isExpanded()) {
            itemList.add(buildMenuListItem(R.string.ntp_turn_off_feed,
                    R.id.ntp_feed_header_menu_item_toggle_switch, icon_id));
        } else {
            itemList.add(buildMenuListItem(R.string.ntp_turn_on_feed,
                    R.id.ntp_feed_header_menu_item_toggle_switch, icon_id));
        }
        return itemList;
    }

    /**
     * Callback on sign-in promo is dismissed.
     */
    void onSignInPromoDismissed() {
        View view = mCoordinator.getSigninPromoView();
        mSignInPromo.dismiss(removedItemTitle
                -> view.announceForAccessibility(view.getResources().getString(
                        R.string.ntp_accessibility_item_removed, removedItemTitle)));
    }

    /** Whether a new thumbnail should be captured. */
    boolean shouldCaptureThumbnail() {
        return mStreamContentChanged || mCoordinator.getView().getWidth() != mThumbnailWidth
                || mCoordinator.getView().getHeight() != mThumbnailHeight
                || getVerticalScrollOffset() != mThumbnailScrollY;
    }

    /** Reset all the properties for thumbnail capturing after a new thumbnail is captured. */
    void onThumbnailCaptured() {
        mThumbnailWidth = mCoordinator.getView().getWidth();
        mThumbnailHeight = mCoordinator.getView().getHeight();
        mThumbnailScrollY = getVerticalScrollOffset();
        mStreamContentChanged = false;
    }

    /**
     * @return Whether the touch events are enabled on the {@link FeedNewTabPage}.
     * TODO(huayinz): Move this method to a Model once a Model is introduced.
     */
    boolean getTouchEnabled() {
        return mTouchEnabled;
    }

    // TouchEnabledDelegate interface.

    @Override
    public void setTouchEnabled(boolean enabled) {
        mTouchEnabled = enabled;
    }

    // ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        if (mFeedEnabled) {
            Stream stream = mCoordinator.getStream();
            // During startup the view may not be fully initialized, so we check to see if some
            // basic view properties (height of the RecyclerView) are sane.
            return stream != null && stream.getView().getHeight() > 0;
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            return scrollView != null && scrollView.getHeight() > 0;
        }
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible.
        if (!isScrollViewInitialized()) return 0;

        if (mFeedEnabled) {
            int firstChildTop = mCoordinator.getStream().getChildTopAt(0);
            return firstChildTop != Stream.POSITION_NOT_KNOWN ? -firstChildTop : Integer.MIN_VALUE;
        } else {
            return mCoordinator.getScrollViewForPolicy().getScrollY();
        }
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        if (!isScrollViewInitialized()) return false;

        if (mFeedEnabled) {
            return mCoordinator.getStream().isChildAtPositionVisible(position);
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            Rect rect = new Rect();
            scrollView.getHitRect(rect);
            return scrollView.getChildAt(position).getLocalVisibleRect(rect);
        }
    }

    @Override
    public void snapScroll() {
        if (mSnapScrollHelper == null) return;
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);

        if (mFeedEnabled) {
            mCoordinator.getStream().smoothScrollBy(0, scrollTo - initialScroll);
        } else {
            mCoordinator.getScrollViewForPolicy().smoothScrollBy(0, scrollTo - initialScroll);
        }
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateSectionHeader();
    }

    @Override
    public void onItemSelected(PropertyModel item) {
        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        if (itemId == R.id.ntp_feed_header_menu_item_activity) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://myactivity.google.com/myactivity?product=50"));
        } else if (itemId == R.id.ntp_feed_header_menu_item_interest) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/preferences/interests"));
        } else if (itemId == R.id.ntp_feed_header_menu_item_learn) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/chrome/privacy"));
        } else if (itemId == R.id.ntp_feed_header_menu_item_toggle_switch) {
            mSectionHeader.toggleHeader();
            SuggestionsMetrics.recordExpandableHeaderTapped(mSectionHeader.isExpanded());
            SuggestionsMetrics.recordArticlesListVisible();
        } else {
            assert false
                : String.format("Cannot handle action for item in the menu with id %d", itemId);
        }
    }

    @Override
    public void onHomepagePromoStateChange() {
        mCoordinator.updateHeaderViews(mSignInPromo != null && mSignInPromo.isVisible());
    }

    /**
     * The {@link SignInPromo} for the Feed.
     * TODO(huayinz): Update content and visibility through a ModelChangeProcessor.
     */
    private class FeedSignInPromo extends SignInPromo {
        FeedSignInPromo(SigninManager signinManager) {
            super(signinManager);
            maybeUpdateSignInPromo();
        }

        @Override
        protected void setVisibilityInternal(boolean visible) {
            if (isVisible() == visible) return;

            super.setVisibilityInternal(visible);
            mCoordinator.updateHeaderViews(visible);
            maybeUpdateSignInPromo();
        }

        @Override
        protected void notifyDataChanged() {
            maybeUpdateSignInPromo();
        }

        /** Update the content displayed in {@link PersonalizedSigninPromoView}. */
        private void maybeUpdateSignInPromo() {
            // Only call #setupPromoViewFromCache() if SignInPromo is visible to avoid potentially
            // blocking the UI thread for several seconds if the accounts cache is not populated
            // yet.
            if (!isVisible()) return;
            SigninPromoUtil.setupPromoViewFromCache(mSigninPromoController, mProfileDataCache,
                    mCoordinator.getSigninPromoView(), null);
        }
    }

    // TODO(huayinz): Return the Model for testing in Coordinator instead once a Model is created.
    @VisibleForTesting
    SectionHeader getSectionHeaderForTesting() {
        return mSectionHeader;
    }

    @VisibleForTesting
    SignInPromo getSignInPromoForTesting() {
        return mSignInPromo;
    }
}
