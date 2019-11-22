// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.support.v4.view.ViewCompat;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.ui.BorderDrawable;
import com.google.android.libraries.feed.piet.ui.GradientDrawable;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.android.libraries.feed.piet.ui.RoundedCornerViewHelper;
import com.google.android.libraries.feed.piet.ui.RoundedCornerWrapperView;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.GradientsProto.Fill;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.Borders;
import com.google.search.now.ui.piet.StylesProto.Borders.Edges;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.Style.HeightSpecCase;

/**
 * Class which understands how to create the current styles, converting from Piet to Android values
 * as desired (ex. Piet gravity to Android gravity).
 *
 * <p>Feel free to add "has..." methods for any of the fields if needed.
 */
class StyleProvider {
    /**
     * Value returned by get(Height|Width)SpecPx when an explicit value for the dimension has not
     * been computed, and it should be set by the parent instead.
     */
    public static final int DIMENSION_NOT_SET = -3;

    // For borders with no rounded corners.
    private static final float[] ZERO_RADIUS = new float[] {0, 0, 0, 0, 0, 0, 0, 0};

    private final Style style;
    private final AssetProvider assetProvider;

    StyleProvider(AssetProvider assetProvider) {
        this.style = Style.getDefaultInstance();
        this.assetProvider = assetProvider;
    }

    StyleProvider(Style style, AssetProvider assetProvider) {
        this.style = style;
        this.assetProvider = assetProvider;
    }

    /** Default font or foreground color */
    public int getColor() {
        return style.getColor();
    }

    /** Whether a color is explicitly specified */
    public boolean hasColor() {
        return style.hasColor();
    }

    /** This style's background */
    public Fill getBackground() {
        return style.getBackground();
    }

    /** The pre-load fill for this style */
    public Fill getPreLoadFill() {
        return style.getImageLoadingSettings().getPreLoadFill();
    }

    /** Whether the style has a pre-load fill */
    public boolean hasPreLoadFill() {
        return style.getImageLoadingSettings().hasPreLoadFill();
    }

    /** Whether to fade in the image after it's loaded */
    public boolean getFadeInImageOnLoad() {
        return style.getImageLoadingSettings().getFadeInImageOnLoad();
    }

    /** Image scale type on this style */
    public ImageView.ScaleType getScaleType() {
        switch (style.getScaleType()) {
            case CENTER_CROP:
                return ScaleType.CENTER_CROP;
            case CENTER_INSIDE:
            case SCALE_TYPE_UNSPECIFIED:
                return ScaleType.FIT_CENTER;
        }
        throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                String.format("Unsupported ScaleType: %s", style.getScaleType()));
    }

    /** The {@link RoundedCorners} to be used with the background color. */
    public RoundedCorners getRoundedCorners() {
        return style.getRoundedCorners();
    }

    /** Whether rounded corners are explicitly specified */
    public boolean hasRoundedCorners() {
        if (!style.hasRoundedCorners()) {
            return false;
        }
        RoundedCorners roundedCorners = style.getRoundedCorners();
        int radiusOverride = roundedCorners.getUseHostRadiusOverride()
                ? assetProvider.getDefaultCornerRadius()
                : 0;
        return RoundedCornerViewHelper.hasValidRoundedCorners(roundedCorners, radiusOverride);
    }

    /** The font for this style */
    public Font getFont() {
        return style.getFont();
    }

    /** This style's padding */
    public EdgeWidths getPadding() {
        return style.getPadding();
    }

    /** This style's margins */
    public EdgeWidths getMargins() {
        return style.getMargins();
    }

    /** Whether this style has borders */
    public boolean hasBorders() {
        return style.getBorders().getWidth() > 0;
    }

    /** This style's borders */
    public Borders getBorders() {
        return style.getBorders();
    }

    /** The max_lines for a TextView */
    public int getMaxLines() {
        return style.getMaxLines();
    }

    /** The min_height for a view */
    public int getMinHeight() {
        return style.getMinHeight();
    }

    /**
     * Height of a view in px, or {@link LayoutParams#WRAP_CONTENT} or {@link
     * LayoutParams#MATCH_PARENT}, or {@link StyleProvider#DIMENSION_NOT_SET} when not defined.
     */
    public int getHeightSpecPx(Context context) {
        switch (style.getHeightSpecCase()) {
            case HEIGHT:
                return (int) LayoutUtils.dpToPx(style.getHeight(), context);
            case RELATIVE_HEIGHT:
                switch (style.getRelativeHeight()) {
                    case FILL_PARENT:
                        return LayoutParams.MATCH_PARENT;
                    case FIT_CONTENT:
                        return LayoutParams.WRAP_CONTENT;
                    case RELATIVE_SIZE_UNDEFINED:
                        // fall through
                }
                // fall through
            case HEIGHTSPEC_NOT_SET:
                // fall out
        }
        return DIMENSION_NOT_SET;
    }

    /**
     * Width of a view in px, or {@link LayoutParams#WRAP_CONTENT} or {@link
     * LayoutParams#MATCH_PARENT}, or {@link StyleProvider#DIMENSION_NOT_SET} when not defined.
     */
    public int getWidthSpecPx(Context context) {
        switch (style.getWidthSpecCase()) {
            case WIDTH:
                return (int) LayoutUtils.dpToPx(style.getWidth(), context);
            case RELATIVE_WIDTH:
                switch (style.getRelativeWidth()) {
                    case FILL_PARENT:
                        return LayoutParams.MATCH_PARENT;
                    case FIT_CONTENT:
                        return LayoutParams.WRAP_CONTENT;
                    case RELATIVE_SIZE_UNDEFINED:
                        // fall through
                }
                // fall through
            case WIDTHSPEC_NOT_SET:
                // fall out
        }
        return DIMENSION_NOT_SET;
    }

    /** Whether a height is explicitly specified */
    public boolean hasHeight() {
        return style.getHeightSpecCase() != HeightSpecCase.HEIGHTSPEC_NOT_SET;
    }

    /** Whether a width is explicitly specified */
    public boolean hasWidth() {
        return style.getHeightSpecCase() != HeightSpecCase.HEIGHTSPEC_NOT_SET;
    }

    /** Whether the style has a horizontal gravity specified */
    public boolean hasGravityHorizontal() {
        return style.hasGravityHorizontal();
    }

    /** Horizontal gravity specified on the style */
    public int getGravityHorizontal(int defaultGravity) {
        switch (style.getGravityHorizontal()) {
            case GRAVITY_START:
                return Gravity.START;
            case GRAVITY_CENTER:
                return Gravity.CENTER_HORIZONTAL;
            case GRAVITY_END:
                return Gravity.END;
            case GRAVITY_HORIZONTAL_UNSPECIFIED:
                // fall out
        }
        return defaultGravity;
    }

    /** Whether the style has a vertical gravity specified */
    public boolean hasGravityVertical() {
        return style.hasGravityVertical();
    }

    /** Vertical gravity specified on the style */
    public int getGravityVertical(int defaultGravity) {
        switch (style.getGravityVertical()) {
            case GRAVITY_TOP:
                return Gravity.TOP;
            case GRAVITY_MIDDLE:
                return Gravity.CENTER_VERTICAL;
            case GRAVITY_BOTTOM:
                return Gravity.BOTTOM;
            case GRAVITY_VERTICAL_UNSPECIFIED:
                // fall out
        }
        return defaultGravity;
    }

    /** Gravity (horizontal and vertical) as specified on the style */
    public int getGravity(int defaultGravity) {
        return getGravityHorizontal(defaultGravity) | getGravityVertical(defaultGravity);
    }

    /** Horizontal and vertical alignment of text, as an Android view Gravity */
    public int getTextAlignment() {
        int horizontalGravity;
        int verticalGravity;

        switch (style.getTextAlignmentHorizontal()) {
            case TEXT_ALIGNMENT_CENTER:
                horizontalGravity = Gravity.CENTER_HORIZONTAL;
                break;
            case TEXT_ALIGNMENT_END:
                horizontalGravity = Gravity.END;
                break;
            case TEXT_ALIGNMENT_START:
            default:
                horizontalGravity = Gravity.START;
        }

        switch (style.getTextAlignmentVertical()) {
            case TEXT_ALIGNMENT_MIDDLE:
                verticalGravity = Gravity.CENTER_VERTICAL;
                break;
            case TEXT_ALIGNMENT_BOTTOM:
                verticalGravity = Gravity.BOTTOM;
                break;
            case TEXT_ALIGNMENT_TOP:
            default:
                verticalGravity = Gravity.TOP;
        }

        return horizontalGravity | verticalGravity;
    }

    /** Sets the Padding and Background Color on the view. */
    void applyElementStyles(ElementAdapter<?, ?> adapter) {
        Context context = adapter.getContext();
        View view = adapter.getView();
        View baseView = adapter.getBaseView();

        // Apply layout styles
        EdgeWidths padding = getPadding();

        // If this is a TextElementAdapter with line height set, extra padding needs to be added to
        // the top and bottom to match the css line height behavior.
        TextElementAdapter.ExtraLineHeight extraLineHeight =
                TextElementAdapter.ExtraLineHeight.builder().build();
        if (adapter instanceof TextElementAdapter) {
            extraLineHeight = ((TextElementAdapter) adapter).getExtraLineHeight();
        }

        applyPadding(context, baseView, padding, extraLineHeight);

        if (getMinHeight() > 0) {
            baseView.setMinimumHeight((int) LayoutUtils.dpToPx(getMinHeight(), context));
        } else {
            baseView.setMinimumHeight(0);
        }

        // Apply appearance styles
        baseView.setBackground(createBackground());
        if (style.getShadow().hasElevationShadow()) {
            ViewCompat.setElevation(view, style.getShadow().getElevationShadow().getElevation());
        } else {
            ViewCompat.setElevation(view, 0.0f);
        }
        if (view != baseView) {
            ViewCompat.setElevation(baseView, 0.0f);
        }

        baseView.setAlpha(style.getOpacity());
    }

    /**
     * Set the padding on a view. This includes adding extra padding for line spacing on TextView
     * and extra padding for borders.
     */
    void applyPadding(Context context, View view, EdgeWidths padding,
            TextElementAdapter.ExtraLineHeight extraLineHeight) {
        int startPadding =
                (int) LayoutUtils.dpToPx(padding.getStart() + getBorderWidth(Edges.START), context);
        int endPadding =
                (int) LayoutUtils.dpToPx(padding.getEnd() + getBorderWidth(Edges.END), context);
        int topPadding =
                (int) LayoutUtils.dpToPx(padding.getTop() + getBorderWidth(Edges.TOP), context)
                + extraLineHeight.topPaddingPx();
        int bottomPadding = (int) LayoutUtils.dpToPx(
                                    padding.getBottom() + getBorderWidth(Edges.BOTTOM), context)
                + extraLineHeight.bottomPaddingPx();

        view.setPadding(assetProvider.isRtL() ? endPadding : startPadding, topPadding,
                assetProvider.isRtL() ? startPadding : endPadding, bottomPadding);
    }

    private int getBorderWidth(Borders.Edges edge) {
        if (!hasBorders()) {
            return 0;
        }
        if (getBorders().getBitmask() == 0
                || ((getBorders().getBitmask() & edge.getNumber()) != 0)) {
            return getBorders().getWidth();
        }
        return 0;
    }

    void addBordersWithoutRoundedCorners(FrameLayout view, Context context) {
        if (!hasBorders()) {
            return;
        }

        // Create a drawable to stroke the border
        BorderDrawable borderDrawable =
                new BorderDrawable(context, getBorders(), ZERO_RADIUS, assetProvider.isRtL());
        view.setForeground(borderDrawable);
    }

    /** Sets appropriate margins on a {@link MarginLayoutParams} instance. */
    void applyMargins(Context context, MarginLayoutParams marginLayoutParams) {
        EdgeWidths margins = getMargins();
        int startMargin = (int) LayoutUtils.dpToPx(margins.getStart(), context);
        int endMargin = (int) LayoutUtils.dpToPx(margins.getEnd(), context);
        marginLayoutParams.setMargins(startMargin,
                (int) LayoutUtils.dpToPx(margins.getTop(), context), endMargin,
                (int) LayoutUtils.dpToPx(margins.getBottom(), context));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            marginLayoutParams.setMarginStart(startMargin);
            marginLayoutParams.setMarginEnd(endMargin);
        }
    }

    FrameLayout createWrapperView(Context context, RoundedCornerMaskCache maskCache,
            boolean allowClipPathRounding, boolean allowOutlineRounding) {
        if (!hasRoundedCorners()) {
            FrameLayout view = new FrameLayout(context);
            if (Build.VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
                // The wrapper view gets elevation; set an outline so it can cast a shadow.
                view.setOutlineProvider(ViewOutlineProvider.BOUNDS);
            }
            return view;
        }
        int radiusOverride = getRoundedCorners().getUseHostRadiusOverride()
                ? assetProvider.getDefaultCornerRadius()
                : 0;

        return new RoundedCornerWrapperView(context, getRoundedCorners(), maskCache,
                assetProvider.isRtLSupplier(), radiusOverride, getBorders(), allowClipPathRounding,
                allowOutlineRounding);
    }

    /**
     * Return a {@link Drawable} with the fill and rounded corners defined on the style; returns
     * {@code null} if the background has no color defined.
     */
    /*@Nullable*/
    Drawable createBackground() {
        return createBackgroundForFill(getBackground());
    }

    /**
     * Return a {@link Drawable} with the fill and rounded corners defined on the style; returns
     * {@code null} if the pre load fill has no color defined.
     */
    /*@Nullable*/
    Drawable createPreLoadFill() {
        return createBackgroundForFill(getPreLoadFill());
    }

    /**
     * Return a {@link Drawable} with a given Fill and the rounded corners defined on the style;
     * returns {@code null} if the background has no color defined.
     */
    /*@Nullable*/
    private Drawable createBackgroundForFill(Fill background) {
        switch (background.getFillTypeCase()) {
            case COLOR:
                return new ColorDrawable(background.getColor());
            case LINEAR_GRADIENT:
                return new GradientDrawable(
                        background.getLinearGradient(), assetProvider.isRtLSupplier());
            default:
                return null;
        }
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof StyleProvider)) {
            return false;
        }

        StyleProvider that = (StyleProvider) o;

        return style.equals(that.style) && assetProvider.equals(that.assetProvider);
    }

    @Override
    public int hashCode() {
        return style.hashCode();
    }
}
