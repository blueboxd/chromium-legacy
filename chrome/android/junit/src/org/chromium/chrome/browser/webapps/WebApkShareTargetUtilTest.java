// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;

/**
 * Tests WebApkShareTargetUtil.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {WebApkShareTargetUtilTest.WebApkShareTargetUtilShadow.class})
public class WebApkShareTargetUtilTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    private void registerWebApk(Bundle shareTargetBundle) {
        Bundle appBundle = new Bundle();
        appBundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, appBundle, new Bundle[] {shareTargetBundle});
    }

    private static Intent createBasicShareIntent() {
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        return intent;
    }

    private static void assertPostData(WebApkShareTargetUtil.PostData postData, String[] names,
            boolean[] isValueFileUris, String[] values, String[] fileNames, String[] types) {
        Assert.assertNotNull(postData);

        Assert.assertNotNull(postData.names);
        Assert.assertEquals(postData.names.size(), names.length);
        for (int i = 0; i < names.length; i++) {
            Assert.assertEquals(postData.names.get(i), names[i]);
        }

        Assert.assertNotNull(postData.isValueFileUri);
        Assert.assertEquals(postData.isValueFileUri.size(), isValueFileUris.length);
        for (int i = 0; i < isValueFileUris.length; i++) {
            Assert.assertEquals(postData.isValueFileUri.get(i), isValueFileUris[i]);
        }

        Assert.assertNotNull(postData.values);
        Assert.assertEquals(postData.values.size(), values.length);
        for (int i = 0; i < values.length; i++) {
            Assert.assertEquals(new String(postData.values.get(i)), values[i]);
        }

        Assert.assertNotNull(postData.filenames);
        Assert.assertEquals(postData.filenames.size(), fileNames.length);
        for (int i = 0; i < fileNames.length; i++) {
            Assert.assertEquals(postData.filenames.get(i), fileNames[i]);
        }

        Assert.assertNotNull(postData.types);
        Assert.assertEquals(postData.types.size(), types.length);
        for (int i = 0; i < types.length; i++) {
            Assert.assertEquals(postData.types.get(i), types[i]);
        }
    }

    @Implements(WebApkShareTargetUtil.class)
    public static class WebApkShareTargetUtilShadow extends WebApkShareTargetUtil {
        @Implementation
        public static byte[] readStringFromContentUri(Uri uri) {
            return String.format("content-for-%s", uri.toString()).getBytes();
        }

        @Implementation
        public static String getFileTypeFromContentUri(Uri uri) {
            String uriString = uri.toString();
            if (uriString.startsWith("text")) {
                return "text/plain";
            }
            return "image/gif";
        }

        @Implementation
        public static String getFileNameFromContentUri(Uri uri) {
            return String.format("file-name-for-%s", uri.toString());
        }
    }

    /**
     * Test that post data is null when the share method is GET.
     */
    @Test
    public void testGET() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "GET");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        WebApkInfo infoWithShareMethodGet = WebApkInfo.create(intent);

        Assert.assertEquals(null,
                WebApkShareTargetUtilShadow.computePostData(
                        WebApkTestHelper.getGeneratedShareTargetActivityClassName(0),
                        infoWithShareMethodGet.shareTarget(), infoWithShareMethodGet.shareData()));

        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        registerWebApk(shareActivityBundle);
        // recreating the WebApkInfo because the shareActivityBundle used for registerWebApk() has
        // changed
        WebApkInfo infoWithShareMethodPost = WebApkInfo.create(intent);

        Assert.assertNotEquals(null,
                WebApkShareTargetUtilShadow.computePostData(
                        WebApkTestHelper.getGeneratedShareTargetActivityClassName(0),
                        infoWithShareMethodPost.shareTarget(),
                        infoWithShareMethodPost.shareData()));
    }

    /**
     * Test that post data for application/x-www-form-urlencoded will contain
     * the correct information in the correct place.
     */
    @Test
    public void testPostUrlEncoded() throws UnsupportedEncodingException {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "extra_subject");
        intent.putExtra(Intent.EXTRA_TEXT, "extra_text");
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"title", "text"}, new boolean[] {false, false},
                new String[] {"extra_subject", "extra_text"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that
     * multipart/form-data with no names/accepts output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoNamesNoAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");
        // Note that names and accepts are not specified
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-1"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {}, new boolean[] {}, new String[] {},
                new String[] {}, new String[] {});
    }

    /**
     * Test that multipart/form-data with no files or text specified in Intent.EXTRA_STREAM will
     * output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoFilesNorText() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        // Intent.EXTRA_STREAM is not specified.
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {}, new boolean[] {}, new String[] {},
                new String[] {}, new String[] {});
    }

    @Test
    public void testPostMultipartWithFiles() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-2"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"name"}, new boolean[] {true},
                new String[] {"mock-uri-2"}, new String[] {"file-name-for-mock-uri-2"},
                new String[] {"image/gif"});
    }

    @Test
    public void testPostMultipartWithTexts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithTextsOnlyTarget() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithFileAndTexts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-3"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text", "name"},
                new boolean[] {false, false, true},
                new String[] {"shared_subject_value", "shared_text_value", "mock-uri-3"},
                new String[] {"", "", "file-name-for-mock-uri-3"},
                new String[] {"text/plain", "text/plain", "image/gif"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, but we receive a text file, and that
     * we don't receive shared text, that we send the text file as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeNotInAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text"}, new boolean[] {true},
                new String[] {"text-file-mock-uri"}, new String[] {""},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, but we receive multiple text files,
     * and that we don't receive shared text, that we send only one text file as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeNotInAcceptsMultiple() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        uris.add(Uri.parse("text-file-mock-uri2"));
        uris.add(Uri.parse("text-file-mock-uri3"));

        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text"}, new boolean[] {true},
                new String[] {"text-file-mock-uri"}, new String[] {""},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, and that we DO receive shared text;
     * even though we received a text file, we should ignore it, because in the end, a web page
     * expects a single value (not an array) in the "share-text" field.
     */
    @Test
    public void testPostMultipartSharedTextFileAndSharedSelection() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text"}, new boolean[] {false},
                new String[] {"shared_text_value"}, new String[] {""}, new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS DOES accept text, we don't accidentally send the text file
     * as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeInAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"share-text-file\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"text/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text-file"}, new boolean[] {true},
                new String[] {"text-mock-uri"}, new String[] {"file-name-for-text-mock-uri"},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is missing but we receive a text selection, we send it as a
     * file, along with other files.
     */
    @Test
    public void testPostMultipartSharedTextSelectionNoParamTextPlainInAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"share-text-file\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"text/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text-file", "share-text-file"},
                new boolean[] {false, true}, new String[] {"shared_text_value", "text-mock-uri"},
                new String[] {"shared.txt", "file-name-for-text-mock-uri"},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is present and  we receive a text selection, we don't
     * mistakenly send it as a file. File sharing should not be affected either.
     */
    @Test
    public void testPostMultipartSharedTextSelectionHasParamText() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"share-text-file\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"text/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text", "share-text-file"},
                new boolean[] {false, true}, new String[] {"shared_text_value", "text-mock-uri"},
                new String[] {"", "file-name-for-text-mock-uri"},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is missing, we receive a text selection, and we can't find a
     * SHARE_PARAM_ACCEPTS that matches text (such as "text/plain" or "text/*"), we don't mistakenly
     * send the text as a file. In addition, file sharing should not be affected.
     *
     * Ideally this should never happens if the WebAPK Minting server minted WebAPK correctly.
     */
    @Test
    public void testPostMultipartSharedTextSelectionNoParamTextPlainNotInAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"share-text-file\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        assertPostData(postData, new String[] {"share-text-file"}, new boolean[] {true},
                new String[] {"mock-uri"}, new String[] {"file-name-for-mock-uri"},
                new String[] {"image/gif"});
    }

    @Test
    public void testPostMultipartWithFileAndInValidParamNames() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "not a array");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ACTION, "/share.html");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri"));
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData = WebApkShareTargetUtilShadow.computePostData(
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0), info.shareTarget(),
                info.shareData());

        // with invalid name parameter from Android manifest, we ignore the file sharing part.
        assertPostData(postData, new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }
}
