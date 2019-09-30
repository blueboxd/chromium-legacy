// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.aboutui.CreditUtilsJni;

import java.io.FileNotFoundException;

/**
 * Content provider for the OSS licenses file used on Monochrome and Trichrome.
 */
@TargetApi(Build.VERSION_CODES.N)
public class LicenseContentProvider
        extends ContentProvider implements ContentProvider.PipeDataWriter<String> {
    public static final String LICENSES_URI_SUFFIX = "LicenseContentProvider/webview_licenses";
    public static final String LICENSES_CONTENT_TYPE = "text/html";

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            return openPipeHelper(null, null, null, "webview_licenses.notice", this);
        }
        return null;
    }

    @Override
    public void writeDataToPipe(
            ParcelFileDescriptor output, Uri uri, String mimeType, Bundle opts, String filename) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeBrowserInitializer.getInstance(getContext()).handleSynchronousStartup();
            }
        });
        CreditUtilsJni.get().writeCreditsHtml(output.detachFd());
    }

    @Override
    public String getType(Uri uri) {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            return LICENSES_CONTENT_TYPE;
        }
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }
}
