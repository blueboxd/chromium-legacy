// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Rect;
import android.graphics.drawable.shapes.RoundRectShape;
import android.os.Build;
import android.os.Build.VERSION;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.view.ViewParent;
import android.widget.FrameLayout;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerDelegateFactory.RoundingStrategy;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners.RadiusOptionsCase;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders;

/**
 * Wrapper for {@link View} instances in Piet that require rounded corners.
 *
 * <p>There are three strategies used to round corners in different situations. This class decides
 * which to use based on the following rules:
 *
 * <p>OUTLINE_PROVIDER: This is always used when possible (unless disallowed by the host). It
 * requires hardware acceleration, does not work if 1-3 corners are rounded, and does not work on
 * K-.
 *
 * <p>CLIP_PATH: This works in all situations, but does not support anti-aliasing. Used when the
 * device does not support hardware acceleration, and in K- where outline provider is not supported.
 * Although BITMAP_MASKING would work for those cases, this strategy is much more performant. Older
 * SDKs or no hw acceleration support typically are signs of less-performant phones. The lack of
 * anti-aliasing does not negatively effect metrics for these users. This strategy can be disallowed
 * by the host, in which case BITMAP_MASKING will be used where OUTLINE_PROVIDER is not possible.
 *
 * <p>BITMAP_MASKING: This implementation should work in all cases for all SDKs, but has the worst
 * performance in CPU and memory usage. It is only used when 1-3 corners are rounded in L+, or if
 * the other strategies are disallowed by the host or not possible.
 *
 * <p>A {@link RoundedCornerDelegate} is called in places where special logic is needed for
 * different rounding strategies. Each strategy has its own delegate, which is created when the
 * strategy is selected. This class doesn't know about the implementation details of the rounding
 * strategies.
 */
public class RoundedCornerWrapperView extends FrameLayout {
    private final int mRadiusOverride;
    private final RoundedCorners mRoundedCorners;
    private final Supplier<Boolean> mIsRtLSupplier;
    private final Context mContext;
    private final Borders mBorders;
    private final boolean mAllowClipPath;
    private final boolean mAllowOutlineRounding;
    private final boolean mHasRoundedCorners;
    private final boolean mAllFourCornersRounded;
    private final RoundedCornerMaskCache mMaskCache;

    private RoundedCornerDelegate mRoundingDelegate;
    private boolean mDrawSuperCalled;

    // Keep track of current mask configuration so we can use cached values if nothing has changed.
    int mLastWidth = -1;
    int mLastHeight = -1;

    private int mRoundedCornerRadius;

    // Doesn't like the call to setOutlineProvider
    @SuppressWarnings("initialization")
    public RoundedCornerWrapperView(Context context, RoundedCorners roundedCorners,
            RoundedCornerMaskCache maskCache, Supplier<Boolean> isRtLSupplier, int radiusOverride,
            Borders borders, boolean allowClipPath, boolean allowOutlineRounding) {
        super(context);

        this.mRadiusOverride = radiusOverride;
        this.mRoundedCorners = roundedCorners;
        this.mIsRtLSupplier = isRtLSupplier;
        this.mAllowClipPath = allowClipPath;
        this.mAllowOutlineRounding = allowOutlineRounding;
        this.mContext = context;
        this.mBorders = borders;
        this.mMaskCache = maskCache;
        this.mHasRoundedCorners =
                RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, radiusOverride);
        this.mAllFourCornersRounded =
                RoundedCornerViewHelper.allFourCornersRounded(roundedCorners.getBitmask());

        setRoundingStrategy();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Even when not using the outline provider strategy, the outline needs to be set for
            // shadows to render properly.
            setupOutlineProvider();
        }
        setWillNotDraw(false);
    }

    private void setRoundingStrategy() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP && mAllowClipPath) {
            setRoundingDelegate(RoundingStrategy.CLIP_PATH);
            if (VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
                // clipPath doesn't work with hardware rendering on < 18.
                setLayerType(LAYER_TYPE_SOFTWARE, null);
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && mAllFourCornersRounded
                && mAllowOutlineRounding) {
            setRoundingDelegate(RoundingStrategy.OUTLINE_PROVIDER);
        } else {
            setRoundingDelegate(RoundingStrategy.BITMAP_MASKING);
        }
    }

    private void updateRoundingStrategy() {
        if (mRoundingDelegate instanceof OutlineProviderRoundedCornerDelegate
                && this.isAttachedToWindow() && !this.isHardwareAccelerated()) {
            if (mAllowClipPath) {
                setRoundingDelegate(RoundingStrategy.CLIP_PATH);
            } else {
                setRoundingDelegate(RoundingStrategy.BITMAP_MASKING);
            }
        }
    }

    void setRoundingDelegate(RoundingStrategy strategy) {
        if (mRoundingDelegate != null) {
            mRoundingDelegate.destroy(this);
        }
        mRoundingDelegate = RoundedCornerDelegateFactory.getDelegate(
                strategy, mMaskCache, mRoundedCorners.getBitmask(), mIsRtLSupplier.get());
        mRoundingDelegate.initializeForView(this);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void setupOutlineProvider() {
        if (mHasRoundedCorners) {
            super.setOutlineProvider(new ViewOutlineProvider() {
                @Override
                public void getOutline(View view, Outline outline) {
                    int radius = getRadius(view.getWidth(), view.getHeight());
                    if (mAllFourCornersRounded) {
                        outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), radius);
                        return;
                    }
                    float[] radii = RoundedCornerViewHelper.createRoundedCornerBitMask(
                            radius, mRoundedCorners.getBitmask(), mIsRtLSupplier.get());
                    RoundRectShape outlineShape = new RoundRectShape(radii, null, null);
                    outlineShape.resize(view.getWidth(), view.getHeight());
                    // This actually sets the outline to use this shape
                    outlineShape.getOutline(outline);
                }
            });
        } else {
            super.setOutlineProvider(ViewOutlineProvider.BOUNDS);
        }
    }

    /**
     * Calls the rounding delegate to perform any additional work specific to a certain rounding
     * strategy during dispatchDraw().
     */
    @Override
    protected void dispatchDraw(Canvas canvas) {
        mRoundingDelegate.dispatchDraw(canvas);
        super.dispatchDraw(canvas);
    }

    /**
     * Allows the {@link RoundedCornerDelegate} to add logic during onDescendantInvalidated().
     *
     * <p>The bitmap masking rounded corner strategy requires this hook, to make sure the wrapper
     * view is invalidated when there are animations. The delegate handles that logic.
     */
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        mRoundingDelegate.onDescendantInvalidated(this, target);
    }

    /**
     * Allows the {@link RoundedCornerDelegate} to add logic during invalidateChildInParent().
     *
     * <p>The bitmap masking rounded corner strategy requires this hook, to make sure the wrapper
     * view is invalidated when there are animations. The delegate handles that logic.
     */
    @Override
    public ViewParent invalidateChildInParent(final int[] location, final Rect dirty) {
        mRoundingDelegate.invalidateChildInParent(this, dirty);
        return super.invalidateChildInParent(location, dirty);
    }

    /**
     * Calls the {@link RoundedCornerDelegate} to draw to the {@link Canvas}, in case extra logic is
     * needed to round the corners.
     *
     * <p>Draws as normal for everything except the bitmap masking strategy. The bitmap masking
     * strategy requires manipulating the {@link Canvas}, which the delegate handles.
     */
    @Override
    @SuppressWarnings("MissingSuperCall")
    public void draw(Canvas canvas) {
        mDrawSuperCalled = false;
        mRoundingDelegate.draw(this, canvas);
        checkState(mDrawSuperCalled, "View.draw() never called in RoundedCornerWrapperView.draw()");
    }

    public void drawSuper(Canvas canvas) {
        mDrawSuperCalled = true;
        super.draw(canvas);
    }

    /**
     * Updates rounded corner information when the size changes.
     *
     * <p>At this point, the view will actually be attached to the window, meaning we can detect
     * whether it is hardware accelerated. The outline provider strategy doesn't work without
     * hardware acceleration, so the rounding strategy may need to be updated. The size always
     * changes at least once--from 0 to final dimensions--so it is safe to assume that this will
     * catch devices without hw acceleration.
     *
     * <p>Call the rounding delegate to perform any additional work specific to a certain rounding
     * strategy.
     */
    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        updateRoundingStrategy();
        mRoundingDelegate.onSizeChanged(getRadius(width, height), width, height,
                mRoundedCorners.getBitmask(), mIsRtLSupplier.get());
    }

    /**
     * Lays out the view, calling the rounding delegate to perform any additional work specific to a
     * certain rounding strategy.
     *
     * <p>Borders must be created after the radius is known, so they are added from onLayout().
     */
    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (!changed || !mHasRoundedCorners) {
            return;
        }

        int width = getWidth();
        int height = getHeight();
        if (width == 0 || height == 0) {
            // The view is not visible; no further processing is needed.
            return;
        }

        int radius = getRadius(width, height);
        addBorders(radius);

        mRoundingDelegate.onLayout(radius, mIsRtLSupplier.get(), width, height);
    }

    /**
     * Creates a border drawable and adds it to this view's foreground.
     *
     * <p>This is called from {@link RoundedCornerWrapperView} because when the corners are rounded,
     * the borders also need to be rounded, and the radius can't properly be calculated until the
     * view has been laid out and the height and width are known. From this class, it is easy to
     * make sure the border is created after layout happens, since we are overriding draw. Draw is
     * guaranteed to happen after layout, so we can make sure that the borders are drawn with the
     * appropriate measurements.
     */
    void addBorders(int radius) {
        if (mBorders.getWidth() <= 0) {
            return;
        }
        boolean isRtL = mIsRtLSupplier.get();
        // Set up outline of borders
        float[] outerRadii = RoundedCornerViewHelper.createRoundedCornerBitMask(
                radius, mRoundedCorners.getBitmask(), isRtL);

        // Create a drawable to stroke the border
        BorderDrawable borderDrawable =
                new BorderDrawable(mContext, mBorders, outerRadii, isRtL, mLastWidth, mLastHeight);
        this.setForeground(borderDrawable);
    }

    /**
     * Returns the radius, which is only calculated if necessary.
     *
     * <p>If the width and height are the same as the previous width and height, there's no need to
     * re-calculate, and the previous radius is returned. The width and height are needed for radii
     * calculated as a percentage of width or height, and to make sure that the radius isn't too big
     * for the size of the view.
     */
    public int getRadius(int width, int height) {
        if (!mHasRoundedCorners || width == 0 || height == 0) {
            return 0;
        }
        if (mRadiusOverride > 0) {
            mRoundedCornerRadius = mRadiusOverride;
            return mRoundedCornerRadius;
        }
        if (width == mLastWidth && height == mLastHeight) {
            return mRoundedCornerRadius;
        }
        int radius = makeRadius(width, height);
        mLastWidth = width;
        mLastHeight = height;
        return radius;
    }

    /** Calculates the corner radius, clipping to width or height when necessary. */
    private int makeRadius(int width, int height) {
        int radius;
        RadiusOptionsCase radiusOptions = mRoundedCorners.getRadiusOptionsCase();

        switch (radiusOptions) {
            case RADIUS_DP:
                radius = (int) LayoutUtils.dpToPx(mRoundedCorners.getRadiusDp(), mContext);
                break;
            case RADIUS_PERCENTAGE_OF_HEIGHT:
                radius = mRoundedCorners.getRadiusPercentageOfHeight() * height / 100;
                break;
            case RADIUS_PERCENTAGE_OF_WIDTH:
                radius = mRoundedCorners.getRadiusPercentageOfWidth() * width / 100;
                break;
            default:
                radius = 0;
        }

        mRoundedCornerRadius = RoundedCornerViewHelper.adjustRadiusIfTooBig(
                width, height, radius, mRoundedCorners);
        return mRoundedCornerRadius;
    }
}
