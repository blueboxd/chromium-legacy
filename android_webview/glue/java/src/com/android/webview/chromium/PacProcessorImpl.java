// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.PacProcessor;

import org.chromium.android_webview.AwPacProcessor;
import org.chromium.base.JNIUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;

final class PacProcessorImpl implements PacProcessor {
    AwPacProcessor mProcessor;

    private PacProcessorImpl() {
        JNIUtils.setClassLoader(WebViewChromiumFactoryProvider.class.getClassLoader());
        try {
            LibraryLoader.getInstance().ensureInitialized();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Error initializing WebView library", e);
        }

        // This will set up Chromium environment to run proxy resolver.
        AwPacProcessor.initializeEnvironment();
        mProcessor = AwPacProcessor.getInstance();
    }

    private static class LazyHolder {
        static final PacProcessorImpl sInstance = new PacProcessorImpl();
    }

    public static PacProcessorImpl getInstance() {
        return LazyHolder.sInstance;
    }

    @Override
    public boolean setProxyScript(String script) {
        return mProcessor.setProxyScript(script);
    }

    @Override
    public String findProxyForUrl(String url) {
        return mProcessor.makeProxyRequest(url);
    }
}
