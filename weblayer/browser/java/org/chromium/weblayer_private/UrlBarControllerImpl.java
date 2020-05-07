// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.omnibox.SecurityButtonAnimationDelegate;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.interfaces.UrlBarOptionsKeys;

/**
 *  Implementation of {@link IUrlBarController}.
 */
@JNINamespace("weblayer")
public class UrlBarControllerImpl extends IUrlBarController.Stub {
    public static final float DEFAULT_TEXT_SIZE = 10.0F;
    public static final float MINIMUM_TEXT_SIZE = 5.0F;

    private BrowserImpl mBrowserImpl;
    private long mNativeUrlBarController;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private String getUrlForDisplay() {
        return UrlBarControllerImplJni.get().getUrlForDisplay(mNativeUrlBarController);
    }

    void destroy() {
        UrlBarControllerImplJni.get().deleteUrlBarController(mNativeUrlBarController);
        mNativeUrlBarController = 0;
        mBrowserImpl = null;

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    public UrlBarControllerImpl(BrowserImpl browserImpl, long nativeBrowser) {
        mBrowserImpl = browserImpl;
        mNativeUrlBarController =
                UrlBarControllerImplJni.get().createUrlBarController(nativeBrowser);
    }

    @Override
    public IObjectWrapper /* View */ createUrlBarView(Bundle options) {
        StrictModeWorkaround.apply();
        if (mBrowserImpl == null) {
            throw new IllegalStateException("UrlBarView cannot be created without a valid Browser");
        }
        Context context = mBrowserImpl.getContext();
        if (context == null) throw new IllegalStateException("BrowserFragment not attached yet.");

        UrlBarView urlBarView = new UrlBarView(context, options);
        return ObjectWrapper.wrap(urlBarView);
    }

    protected class UrlBarView
            extends LinearLayout implements BrowserImpl.VisibleSecurityStateObserver {
        private float mTextSize;
        private boolean mShowPageInfoWhenUrlTextClicked;

        // These refer to the resources in the embedder's APK, not WebLayer's.
        private @ColorRes int mUrlTextColor;
        private @ColorRes int mUrlIconColor;

        private TextView mUrlTextView;
        private ImageButton mSecurityButton;
        private final SecurityButtonAnimationDelegate mSecurityButtonAnimationDelegate;

        public UrlBarView(@NonNull Context context, Bundle options) {
            super(context);
            mTextSize = options.getFloat(UrlBarOptionsKeys.URL_TEXT_SIZE, DEFAULT_TEXT_SIZE);
            mShowPageInfoWhenUrlTextClicked = options.getBoolean(
                    UrlBarOptionsKeys.SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED, /*default= */ false);
            mUrlTextColor = options.getInt(UrlBarOptionsKeys.URL_TEXT_COLOR, /*default= */ 0);
            mUrlIconColor = options.getInt(UrlBarOptionsKeys.URL_ICON_COLOR, /*default= */ 0);

            View.inflate(getContext(), R.layout.weblayer_url_bar, this);
            setOrientation(LinearLayout.HORIZONTAL);
            setBackgroundColor(Color.TRANSPARENT);
            mUrlTextView = findViewById(R.id.url_text);
            mSecurityButton = (ImageButton) findViewById(R.id.security_button);
            mSecurityButtonAnimationDelegate = new SecurityButtonAnimationDelegate(
                    mSecurityButton, mUrlTextView, R.dimen.security_status_icon_size);

            updateView();
        }

        // BrowserImpl.VisibleSecurityStateObserver
        @Override
        public void onVisibleSecurityStateOfActiveTabChanged() {
            updateView();
        }

        @Override
        protected void onAttachedToWindow() {
            if (mBrowserImpl != null) {
                mBrowserImpl.addVisibleSecurityStateObserver(this);
                updateView();
            }

            super.onAttachedToWindow();
        }

        @Override
        protected void onDetachedFromWindow() {
            if (mBrowserImpl != null) mBrowserImpl.removeVisibleSecurityStateObserver(this);
            super.onDetachedFromWindow();
        }

        private void updateView() {
            if (mBrowserImpl == null) return;
            String displayUrl =
                    UrlBarControllerImplJni.get().getUrlForDisplay(mNativeUrlBarController);
            mUrlTextView.setText(displayUrl);
            mUrlTextView.setTextSize(
                    TypedValue.COMPLEX_UNIT_SP, Math.max(MINIMUM_TEXT_SIZE, mTextSize));
            Context embedderContext = mBrowserImpl.getEmbedderActivityContext();
            if (mUrlTextColor > 0 && embedderContext != null) {
                mUrlTextView.setTextColor(ContextCompat.getColor(embedderContext, mUrlTextColor));
            }

            mSecurityButtonAnimationDelegate.updateSecurityButton(getSecurityIcon());
            mSecurityButton.setContentDescription(getContext().getResources().getString(
                    SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(
                            UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                                    mNativeUrlBarController))));

            if (mUrlIconColor > 0 && embedderContext != null) {
                ImageViewCompat.setImageTintList(mSecurityButton,
                        ColorStateList.valueOf(
                                ContextCompat.getColor(embedderContext, mUrlIconColor)));
            }

            mSecurityButton.setOnClickListener(v -> { showPageInfoUi(v); });
            if (mShowPageInfoWhenUrlTextClicked) {
                mUrlTextView.setOnClickListener(v -> { showPageInfoUi(v); });
            }
        }

        private void showPageInfoUi(View v) {
            PageInfoController.show(mBrowserImpl.getWindowAndroid().getActivity().get(),
                    mBrowserImpl.getActiveTab().getWebContents(),
                    /* contentPublisher= */ null, PageInfoController.OpenedFromSource.TOOLBAR,
                    new PageInfoControllerDelegate(
                            ((FragmentWindowAndroid)
                                            mBrowserImpl.getWindowAndroid())::getModalDialogManager,
                            new AutocompleteSchemeClassifierImpl(),
                            /** vrHandler= */ null,
                            /** isSiteSettingsAvailable= */ false,
                            /** cookieControlsShown= */ false));
        }

        @DrawableRes
        private int getSecurityIcon() {
            return SecurityStatusIcon.getSecurityIconResource(
                    UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                            mNativeUrlBarController),
                    UrlBarControllerImplJni.get().shouldShowDangerTriangleForWarningLevel(
                            mNativeUrlBarController),
                    mBrowserImpl.isWindowOnSmallDevice(),
                    /* skipIconForNeutralState= */ true);
        }
    }

    @NativeMethods()
    interface Natives {
        long createUrlBarController(long browserPtr);
        void deleteUrlBarController(long urlBarControllerImplPtr);
        String getUrlForDisplay(long nativeUrlBarControllerImpl);
        int getConnectionSecurityLevel(long nativeUrlBarControllerImpl);
        boolean shouldShowDangerTriangleForWarningLevel(long nativeUrlBarControllerImpl);
    }
}
