// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * Provider for processed favicons in Tab list.
 */
public class TabListFaviconProvider {
    private static Drawable sRoundedGlobeDrawable;
    private static Drawable sRoundedChromeDrawable;
    private static Drawable sRoundedComposedDefaultDrawable;
    private final int mFaviconSize;
    private final Context mContext;
    @ColorInt
    private final int mDefaultIconColor;
    @ColorInt
    private final int mIncognitoIconColor;
    private boolean mIsInitialized;

    private Profile mProfile;
    private FaviconHelper mFaviconHelper;

    /**
     * Construct the provider that provides favicons for tab list.
     * @param context The context to use for accessing {@link android.content.res.Resources}
     *
     */
    public TabListFaviconProvider(Context context) {
        mContext = context;
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);

        if (sRoundedGlobeDrawable == null) {
            // TODO(crbug.com/1066709): From Android Developer Documentation, we should avoid
            //  resizing vector drawable.
            Drawable globeDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
            Bitmap globeBitmap =
                    Bitmap.createBitmap(mFaviconSize, mFaviconSize, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(globeBitmap);
            globeDrawable.setBounds(0, 0, mFaviconSize, mFaviconSize);
            globeDrawable.draw(canvas);
            sRoundedGlobeDrawable = processBitmap(globeBitmap);
        }
        if (sRoundedChromeDrawable == null) {
            Bitmap chromeBitmap =
                    BitmapFactory.decodeResource(mContext.getResources(), R.drawable.chromelogo16);
            sRoundedChromeDrawable = processBitmap(chromeBitmap);
        }
        if (sRoundedComposedDefaultDrawable == null) {
            sRoundedComposedDefaultDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.ic_group_icon_16dp);
        }
        mDefaultIconColor = mContext.getResources().getColor(R.color.default_icon_color);
        mIncognitoIconColor = mContext.getResources().getColor(R.color.default_icon_color_light);
    }

    public void initWithNative(Profile profile) {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mProfile = profile;
        mFaviconHelper = new FaviconHelper();
    }

    public boolean isInitialized() {
        return mIsInitialized;
    }

    private Drawable processBitmap(Bitmap bitmap) {
        return FaviconUtils.createRoundedBitmapDrawable(mContext.getResources(),
                Bitmap.createScaledBitmap(bitmap, mFaviconSize, mFaviconSize, true));
    }

    /**
     * @return The scaled rounded Globe Drawable as default favicon.
     * @param isIncognito Whether the {@link Drawable} is used for incognito mode.
     */
    public Drawable getDefaultFaviconDrawable(boolean isIncognito) {
        return getRoundedGlobeDrawable(isIncognito);
    }

    /**
     * Asynchronously get the processed favicon Drawable.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param faviconCallback The callback that requests for favicon.
     */
    public void getFaviconForUrlAsync(
            String url, boolean isIncognito, Callback<Drawable> faviconCallback) {
        if (mFaviconHelper == null || NativePageFactory.isNativePageUrl(url, isIncognito)) {
            faviconCallback.onResult(getRoundedChromeDrawable(isIncognito));
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, url, mFaviconSize, (image, iconUrl) -> {
                        if (image == null) {
                            faviconCallback.onResult(getRoundedGlobeDrawable(isIncognito));
                        } else {
                            faviconCallback.onResult(processBitmap(image));
                        }
                    });
        }
    }

    /**
     * Synchronously get the processed favicon Drawable.
     * @param url The URL whose favicon is requested.
     * @param isIncognito Whether the tab is incognito or not.
     * @param icon The favicon that was received.
     * @return The processed favicon.
     */
    public Drawable getFaviconForUrlSync(String url, boolean isIncognito, Bitmap icon) {
        if (icon == null) {
            boolean isNativeUrl = NativePageFactory.isNativePageUrl(url, isIncognito);
            return isNativeUrl ? getRoundedChromeDrawable(isIncognito)
                               : getRoundedGlobeDrawable(isIncognito);
        } else {
            return processBitmap(icon);
        }
    }

    /**
     * Asynchronously get the composed, up to 4, favicon Drawable.
     * @param urls List of urls, up to 4, whose favicon are requested to be composed.
     * @param isIncognito Whether the processed composed favicon is used for incognito or not.
     * @param faviconCallback The callback that requests for the composed favicon.
     */
    public void getComposedFaviconImageAsync(
            List<String> urls, boolean isIncognito, Callback<Drawable> faviconCallback) {
        assert urls != null && urls.size() > 1 && urls.size() <= 4;

        mFaviconHelper.getComposedFaviconImage(mProfile, urls, mFaviconSize, (image, iconUrl) -> {
            if (image == null) {
                faviconCallback.onResult(getDefaultComposedImage(isIncognito));
            } else {
                faviconCallback.onResult(processBitmap(image));
            }
        });
    }

    private Drawable getDefaultComposedImage(boolean isIncognito) {
        return getTintedDrawable(sRoundedComposedDefaultDrawable, isIncognito);
    }

    private Drawable getRoundedChromeDrawable(boolean isIncognito) {
        return getTintedDrawable(sRoundedChromeDrawable, isIncognito);
    }

    private Drawable getRoundedGlobeDrawable(boolean isIncognito) {
        return getTintedDrawable(sRoundedGlobeDrawable, isIncognito);
    }

    private Drawable getTintedDrawable(Drawable drawable, boolean isIncognito) {
        @ColorInt
        int color = isIncognito ? mIncognitoIconColor : mDefaultIconColor;
        // Since static variable is still loaded when activity is destroyed due to configuration
        // changes, e.g. light/dark theme changes, setColorFilter is needed when we retrieve the
        // drawable. setColorFilter would be a no-op if color and the mode are the same.
        drawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        return drawable;
    }
}
