// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 *  Implementation of {@link IUrlBarController}.
 */
@JNINamespace("weblayer")
public class UrlBarControllerImpl extends IUrlBarController.Stub {
    // To be kept in sync with the constants in UrlBarOptions.java
    public static final String URL_TEXT_SIZE = "UrlTextSize";
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
        private TextView mUrlTextView;
        private ImageButton mSecurityButton;

        public UrlBarView(@NonNull Context context, Bundle options) {
            super(context);
            mTextSize = options.getFloat(URL_TEXT_SIZE, DEFAULT_TEXT_SIZE);
            View.inflate(getContext(), R.layout.weblayer_url_bar, this);
            setOrientation(LinearLayout.HORIZONTAL);
            setBackgroundColor(Color.TRANSPARENT);
            mUrlTextView = findViewById(R.id.url_text);
            mSecurityButton = (ImageButton) findViewById(R.id.security_button);

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

            mSecurityButton.setImageResource(getSecurityIcon());
            mSecurityButton.setContentDescription(getContext().getResources().getString(
                    SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(
                            UrlBarControllerImplJni.get().getConnectionSecurityLevel(
                                    mNativeUrlBarController))));

            // TODO(crbug.com/1025607): Set a click listener to show Page Info UI.
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