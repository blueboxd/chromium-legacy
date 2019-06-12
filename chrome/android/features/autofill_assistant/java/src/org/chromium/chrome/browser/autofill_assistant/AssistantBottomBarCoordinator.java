// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.support.annotation.Nullable;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantSuggestionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestModel;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.ListModel;

/**
 * Coordinator responsible for the Autofill Assistant bottom bar.
 */
class AssistantBottomBarCoordinator
        implements CompositorViewResizer, AssistantPeekHeightCoordinator.Delegate {
    private static final int FADE_OUT_TRANSITION_TIME_MS = 150;
    private static final int FADE_IN_TRANSITION_TIME_MS = 150;
    private static final int CHANGE_BOUNDS_TRANSITION_TIME_MS = 250;

    private final AssistantModel mModel;
    private final BottomSheetController mBottomSheetController;
    private final AssistantBottomSheetContent mContent;

    // Child coordinators.
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantDetailsCoordinator mDetailsCoordinator;
    private final AssistantFormCoordinator mFormCoordinator;
    private final AssistantCarouselCoordinator mSuggestionsCoordinator;
    private final AssistantCarouselCoordinator mActionsCoordinator;
    private final AssistantPeekHeightCoordinator mPeekHeightCoordinator;
    private AssistantInfoBoxCoordinator mInfoBoxCoordinator;
    private AssistantPaymentRequestCoordinator mPaymentRequestCoordinator;

    // The transition triggered whenever the layout of the BottomSheet content changes.
    private final TransitionSet mLayoutTransition =
            new TransitionSet()
                    .setOrdering(TransitionSet.ORDERING_SEQUENTIAL)
                    .addTransition(new Fade(Fade.OUT).setDuration(FADE_OUT_TRANSITION_TIME_MS))
                    .addTransition(new ChangeBounds().setDuration(CHANGE_BOUNDS_TRANSITION_TIME_MS))
                    .addTransition(new Fade(Fade.IN).setDuration(FADE_IN_TRANSITION_TIME_MS));

    private final ObserverList<CompositorViewResizer.Observer> mSizeObservers =
            new ObserverList<>();
    private boolean mResizeViewport;

    AssistantBottomBarCoordinator(
            ChromeActivity activity, AssistantModel model, BottomSheetController controller) {
        mModel = model;
        mBottomSheetController = controller;
        mContent = new AssistantBottomSheetContent(activity);

        // Set up animations. We need to setup them before initializing the child coordinators as we
        // want our observers to be triggered before the coordinators/view binders observers.
        // TODO(crbug.com/806868): We should only animate our BottomSheetContent instead of the root
        // view. However, it looks like doing that is not well supported by the BottomSheet, so the
        // BottomSheet offset is wrong during the animation.
        ViewGroup rootView = (ViewGroup) controller.getBottomSheet().getRootView();
        setupAnimations(model, rootView);

        // Instantiate child components.
        mHeaderCoordinator = new AssistantHeaderCoordinator(
                activity, mContent.mBottomBarView, model.getHeaderModel());
        mInfoBoxCoordinator = new AssistantInfoBoxCoordinator(activity, model.getInfoBoxModel());
        mDetailsCoordinator = new AssistantDetailsCoordinator(activity, model.getDetailsModel());
        mPaymentRequestCoordinator =
                new AssistantPaymentRequestCoordinator(activity, model.getPaymentRequestModel());
        mFormCoordinator = new AssistantFormCoordinator(activity, model.getFormModel());
        mSuggestionsCoordinator =
                new AssistantSuggestionsCarouselCoordinator(activity, model.getSuggestionsModel());
        mActionsCoordinator =
                new AssistantActionsCarouselCoordinator(activity, model.getActionsModel());
        BottomSheet bottomSheet = controller.getBottomSheet();
        mPeekHeightCoordinator = new AssistantPeekHeightCoordinator(activity, this, bottomSheet,
                mContent.mToolbarView, mContent.mBottomBarView, mSuggestionsCoordinator.getView(),
                mActionsCoordinator.getView(), AssistantPeekHeightCoordinator.PeekMode.HANDLE);

        // We don't want to animate the carousels children views as they are already animated by the
        // recyclers ItemAnimator, so we exclude them to avoid a clash between the animations.
        mLayoutTransition.excludeChildren(mSuggestionsCoordinator.getView(), /* exclude= */ true);
        mLayoutTransition.excludeChildren(mActionsCoordinator.getView(), /* exclude= */ true);

        // Add child views to bottom bar container. We put all child views in the scrollable
        // container, except the actions and suggestions.
        mContent.mScrollableContentContainer.addView(mInfoBoxCoordinator.getView());
        mContent.mScrollableContentContainer.addView(mDetailsCoordinator.getView());
        mContent.mScrollableContentContainer.addView(mPaymentRequestCoordinator.getView());
        mContent.mScrollableContentContainer.addView(mFormCoordinator.getView());
        mContent.mBottomBarView.addView(mSuggestionsCoordinator.getView());
        mContent.mBottomBarView.addView(mActionsCoordinator.getView());

        // Set children top margins to have a spacing between them.
        int childSpacing = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_vertical_spacing);
        int suggestionsVerticalInset = activity.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.chip_bg_vertical_inset);
        setChildMarginTop(mDetailsCoordinator.getView(), childSpacing);
        setChildMarginTop(mPaymentRequestCoordinator.getView(), childSpacing);
        setChildMarginTop(mFormCoordinator.getView(), childSpacing);
        setChildMargin(mSuggestionsCoordinator.getView(), childSpacing - suggestionsVerticalInset,
                -suggestionsVerticalInset);

        // Hide the carousels when they are empty.
        hideWhenEmpty(
                mSuggestionsCoordinator.getView(), model.getSuggestionsModel().getChipsModel());
        hideWhenEmpty(mActionsCoordinator.getView(), model.getActionsModel().getChipsModel());

        // Set the horizontal margins of children. We don't set them on the payment request and the
        // carousels to allow them to take the full width of the sheet.
        setHorizontalMargins(mInfoBoxCoordinator.getView());
        setHorizontalMargins(mDetailsCoordinator.getView());
        setHorizontalMargins(mFormCoordinator.getView());

        View bottomSheetContainer =
                bottomSheet.findViewById(org.chromium.chrome.R.id.bottom_sheet_content);
        bottomSheet.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(int reason) {
                // When scrolling with y < peekHeight, the BottomSheet will make the content
                // invisible. This is a workaround to prevent that as our toolbar is transparent and
                // we want to sheet content to stay visible.
                if ((bottomSheet.getSheetState() == BottomSheet.SheetState.SCROLLING
                            || bottomSheet.getSheetState() == BottomSheet.SheetState.PEEK)
                        && bottomSheet.getCurrentSheetContent() == mContent
                        && model.get(AssistantModel.VISIBLE)) {
                    bottomSheetContainer.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheet.SheetState.PEEK
                        && bottomSheet.getCurrentSheetContent() == mContent) {
                    // When in the peek state, the BottomSheet hides the content view. We override
                    // that because we artificially increase the height of the transparent toolbar
                    // to show parts of the content view.
                    bottomSheetContainer.setVisibility(View.VISIBLE);
                }

                maybeShowHeaderChip();
            }

            @Override
            public void onSheetContentChanged(@Nullable BottomSheet.BottomSheetContent newContent) {
                // TODO(crbug.com/806868): Make sure this works and does not interfere with Duet
                // once we are in ChromeTabbedActivity.
                notifyAutofillAssistantSizeChanged();
            }
        });

        // Show or hide the bottom sheet content when the Autofill Assistant visibility is changed.
        model.addObserver((source, propertyKey) -> {
            if (AssistantModel.VISIBLE == propertyKey) {
                if (model.get(AssistantModel.VISIBLE)) {
                    showAndExpand();
                } else {
                    hide();
                }
            } else if (AssistantModel.ALLOW_TALKBACK_ON_WEBSITE == propertyKey) {
                boolean allow = model.get(AssistantModel.ALLOW_TALKBACK_ON_WEBSITE);
                if (allow) {
                    activity.removeViewObscuringAllTabs(bottomSheet);
                } else {
                    activity.addViewObscuringAllTabs(bottomSheet);
                }
            }
        });

        // Don't clip the content scroll view unless it is scrollable. This is necessary for shadows
        // (i.e. details shadow and carousel cancel button shadow) but we need to clip the children
        // when the ScrollView is scrollable, otherwise scrolled content will overlap with the
        // header and carousels.
        ScrollView scrollView = mContent.mScrollableContent;
        scrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean canScroll =
                            scrollView.canScrollVertically(-1) || scrollView.canScrollVertically(1);
                    mContent.mScrollableContent.setClipChildren(canScroll);
                    mContent.mBottomBarView.setClipChildren(canScroll);
                });
    }

    private void setupAnimations(AssistantModel model, ViewGroup rootView) {
        // Animate when the chip in the header changes.
        model.getHeaderModel().addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantHeaderModel.CHIP
                    || propertyKey == AssistantHeaderModel.CHIP_VISIBLE) {
                animateChildren(rootView);
            }
        });

        // Animate when info box changes.
        model.getInfoBoxModel().addObserver((source, propertyKey) -> animateChildren(rootView));

        // Animate when details change.
        model.getDetailsModel().addObserver((source, propertyKey) -> animateChildren(rootView));

        // Animate when a PR section is expanded.
        model.getPaymentRequestModel().addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantPaymentRequestModel.EXPANDED_SECTION) {
                animateChildren(rootView);
            }
        });

        // Animate when form inputs change.
        model.getFormModel().getInputsModel().addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                animateChildren(rootView);
            }
        });
    }

    private void animateChildren(ViewGroup rootView) {
        TransitionManager.beginDelayedTransition(rootView, mLayoutTransition);
    }

    private void maybeShowHeaderChip() {
        boolean showChip = mBottomSheetController.getBottomSheet().getSheetState()
                        == BottomSheet.SheetState.PEEK
                && mPeekHeightCoordinator.getPeekMode()
                        == AssistantPeekHeightCoordinator.PeekMode.HANDLE_HEADER;
        mModel.getHeaderModel().set(AssistantHeaderModel.CHIP_VISIBLE, showChip);
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    public void destroy() {
        mInfoBoxCoordinator.destroy();
        mInfoBoxCoordinator = null;
        mPaymentRequestCoordinator.destroy();
        mPaymentRequestCoordinator = null;
        mHeaderCoordinator.destroy();
    }

    /**
     * Show the onboarding screen and call {@code callback} with {@code true} if the user agreed to
     * proceed, false otherwise.
     */
    public void showOnboarding(String experimentIds, Callback<Boolean> callback) {
        mModel.getHeaderModel().set(AssistantHeaderModel.VISIBLE, false);

        // Show overlay to prevent user from interacting with the page during onboarding.
        mModel.getOverlayModel().set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);

        AssistantOnboardingCoordinator.show(experimentIds, mContent.mBottomBarView.getContext(),
                mContent.mScrollableContentContainer, accepted -> {
                    if (!accepted) {
                        callback.onResult(false);
                        return;
                    }

                    mModel.getHeaderModel().set(AssistantHeaderModel.VISIBLE, true);

                    // Hide overlay.
                    mModel.getOverlayModel().set(
                            AssistantOverlayModel.STATE, AssistantOverlayState.HIDDEN);

                    callback.onResult(true);
                });
    }

    /** Request showing the Assistant bottom bar view and expand the sheet. */
    public void showAndExpand() {
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetController.expandSheet();
        }
    }

    /** Hide the Assistant bottom bar view. */
    public void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    void setResizeViewport(boolean resizeViewport) {
        if (resizeViewport == mResizeViewport) return;

        mResizeViewport = resizeViewport;
        notifyAutofillAssistantSizeChanged();
    }

    /** Set the peek mode. */
    void setPeekMode(@AssistantPeekHeightCoordinator.PeekMode int peekMode) {
        mPeekHeightCoordinator.setPeekMode(peekMode);
        maybeShowHeaderChip();
    }

    @Override
    public void setShowOnlyCarousels(boolean showOnlyCarousels) {
        mContent.mScrollableContent.setVisibility(showOnlyCarousels ? View.GONE : View.VISIBLE);
    }

    @Override
    public void onPeekHeightChanged() {
        notifyAutofillAssistantSizeChanged();
    }

    private void setChildMarginTop(View child, int marginTop) {
        setChildMargin(child, marginTop, 0);
    }

    private void setChildMargin(View child, int marginTop, int marginBottom) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) child.getLayoutParams();
        params.topMargin = marginTop;
        params.bottomMargin = marginBottom;
        child.setLayoutParams(params);
    }

    /**
     * Observe {@code model} such that the associated view is made invisible when it is empty.
     */
    private void hideWhenEmpty(View carouselView, ListModel<AssistantChip> chipsModel) {
        setCarouselVisibility(carouselView, chipsModel);
        chipsModel.addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                setCarouselVisibility(carouselView, chipsModel);
            }
        });
    }

    private void setCarouselVisibility(View carouselView, ListModel<AssistantChip> chipsModel) {
        carouselView.setVisibility(chipsModel.size() > 0 ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    public AssistantCarouselCoordinator getSuggestionsCoordinator() {
        return mSuggestionsCoordinator;
    }

    private void setHorizontalMargins(View view) {
        LinearLayout.MarginLayoutParams layoutParams =
                (LinearLayout.MarginLayoutParams) view.getLayoutParams();
        int horizontalMargin = view.getContext().getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        layoutParams.setMarginStart(horizontalMargin);
        layoutParams.setMarginEnd(horizontalMargin);
        view.setLayoutParams(layoutParams);
    }

    private void notifyAutofillAssistantSizeChanged() {
        int height = getHeight();
        for (Observer observer : mSizeObservers) {
            observer.onHeightChanged(height);
        }
    }

    // Implementation of methods from AutofillAssistantSizeManager.

    @Override
    public int getHeight() {
        if (mResizeViewport
                && mBottomSheetController.getBottomSheet().getCurrentSheetContent() == mContent)
            return mPeekHeightCoordinator.getPeekHeight();

        return 0;
    }

    @Override
    public void addObserver(Observer observer) {
        mSizeObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mSizeObservers.removeObserver(observer);
    }

    // TODO(crbug.com/806868): Move this class at the top of the file once it is a static class.
    private static class AssistantBottomSheetContent implements BottomSheet.BottomSheetContent {
        private final ViewGroup mToolbarView;
        private final SizeListenableLinearLayout mBottomBarView;
        private final ScrollView mScrollableContent;
        private final LinearLayout mScrollableContentContainer;

        public AssistantBottomSheetContent(Context context) {
            mToolbarView = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.autofill_assistant_bottom_sheet_toolbar, /* root= */ null);
            mBottomBarView = (SizeListenableLinearLayout) LayoutInflater.from(context).inflate(
                    R.layout.autofill_assistant_bottom_sheet_content, /* root= */ null);
            mScrollableContent = mBottomBarView.findViewById(R.id.scrollable_content);
            mScrollableContentContainer =
                    mScrollableContent.findViewById(R.id.scrollable_content_container);
        }

        @Override
        public View getContentView() {
            return mBottomBarView;
        }

        @Nullable
        @Override
        public View getToolbarView() {
            return mToolbarView;
        }

        @Override
        public int getVerticalScrollOffset() {
            return mScrollableContent.getScrollY();
        }

        @Override
        public void destroy() {}

        @Override
        public int getPriority() {
            return BottomSheet.ContentPriority.HIGH;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return false;
        }

        @Override
        public boolean isPeekStateEnabled() {
            return true;
        }

        @Override
        public boolean wrapContentEnabled() {
            return true;
        }

        @Override
        public boolean hasCustomLifecycle() {
            return true;
        }

        @Override
        public boolean hasCustomScrimLifecycle() {
            return true;
        }

        @Override
        public boolean setContentSizeListener(@Nullable BottomSheet.ContentSizeListener listener) {
            mBottomBarView.setContentSizeListener(listener);
            return true;
        }

        @Override
        public boolean hideOnScroll() {
            return false;
        }

        @Override
        public int getSheetContentDescriptionStringId() {
            return R.string.autofill_assistant_sheet_content_description;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            return R.string.autofill_assistant_sheet_half_height;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return R.string.autofill_assistant_sheet_full_height;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return R.string.autofill_assistant_sheet_closed;
        }
    }
}
