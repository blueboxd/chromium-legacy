// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.provider.MediaStore;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v4.app.Fragment;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.io.File;

/**
 * Tests that file inputs work as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class InputTypesTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private File mTempFile;

    private class FileIntentInterceptor implements WebLayerShellActivity.IntentInterceptor {
        public Intent mLastIntent;

        private Intent mResponseIntent;
        private int mResultCode = Activity.RESULT_CANCELED;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void interceptIntent(
                Fragment fragment, Intent intent, int requestCode, Bundle options) {
            new Handler().post(() -> {
                fragment.onActivityResult(requestCode, mResultCode, mResponseIntent);
                mLastIntent = intent;
                mCallbackHelper.notifyCalled();
            });
        }

        public void waitForIntent() {
            try {
                mCallbackHelper.waitForCallback(0);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public void setResponse(int resultCode, Intent response) {
            mResponseIntent = response;
            mResultCode = resultCode;
        }
    }

    private FileIntentInterceptor mIntentInterceptor = new FileIntentInterceptor();

    @Before
    public void setUp() throws Exception {
        mTestServer = new EmbeddedTestServer();
        mTestServer.initializeNative(InstrumentationRegistry.getInstrumentation().getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mTestServer.addDefaultHandlers("weblayer/test/data");
        Assert.assertTrue(mTestServer.start(0));

        WebLayerShellActivity activity =
                mActivityTestRule.launchShellWithUrl(mTestServer.getURL("/input_types.html"));
        mTempFile = File.createTempFile("file", null);
        activity.setIntentInterceptor(mIntentInterceptor);

        Intent response = new Intent();
        response.setData(Uri.fromFile(mTempFile));
        mIntentInterceptor.setResponse(Activity.RESULT_OK, response);
    }

    @After
    public void tearDown() {
        mTempFile.delete();
    }

    @Test
    @SmallTest
    public void testFileInputBasic() {
        String id = "input_file";

        openFileInputWithId(id);

        Assert.assertFalse(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputCancel() {
        String id = "input_file";

        // First add a file.
        openFileInputWithId(id);
        waitForNumFiles(id, 1);

        // Now cancel the intent.
        mIntentInterceptor.setResponse(Activity.RESULT_CANCELED, null);
        openFileInputWithId(id);
        waitForNumFiles(id, 0);
    }

    @Test
    @SmallTest
    public void testFileInputText() {
        String id = "input_text";

        openFileInputWithId(id);

        Assert.assertTrue(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputAny() {
        String id = "input_any";

        openFileInputWithId(id);

        Assert.assertFalse(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputMultiple() throws Exception {
        Intent response = new Intent();
        ClipData clipData = ClipData.newUri(mActivityTestRule.getActivity().getContentResolver(),
                "uris", Uri.fromFile(mTempFile));
        File otherTempFile = File.createTempFile("file2", null);
        clipData.addItem(new ClipData.Item(Uri.fromFile(otherTempFile)));
        response.setClipData(clipData);
        mIntentInterceptor.setResponse(Activity.RESULT_OK, response);
        String id = "input_file_multiple";

        openFileInputWithId(id);

        Intent contentIntent = getContentIntent();
        Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
        Assert.assertTrue(contentIntent.hasExtra(Intent.EXTRA_ALLOW_MULTIPLE));

        waitForNumFiles(id, 2);
        otherTempFile.delete();
    }

    @Test
    @SmallTest
    public void testFileInputImage() {
        String id = "input_image";

        openFileInputWithId(id);

        Assert.assertEquals(
                MediaStore.ACTION_IMAGE_CAPTURE, mIntentInterceptor.mLastIntent.getAction());

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputAudio() {
        String id = "input_audio";

        openFileInputWithId(id);

        Assert.assertEquals(MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mIntentInterceptor.mLastIntent.getAction());

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testColorInput() {
        // Just make sure we don't crash when opening the color picker.
        mActivityTestRule.executeScriptSync("var done = false; document.onclick = function() {"
                + "document.getElementById('input_color').click(); done = true;}");
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mActivityTestRule.executeScriptAndExtractBoolean("done"); });
    }

    private void openFileInputWithId(String id) {
        // We need to click the input after user input, otherwise it won't open due to security
        // policy.
        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('" + id + "').click()}");
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        mIntentInterceptor.waitForIntent();
    }

    private void waitForNumFiles(String id, int num) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            return num
                    == mActivityTestRule.executeScriptAndExtractInt(
                            "document.getElementById('" + id + "').files.length");
        });
    }

    private Intent getContentIntent() {
        Assert.assertEquals(Intent.ACTION_CHOOSER, mIntentInterceptor.mLastIntent.getAction());
        Intent contentIntent =
                (Intent) mIntentInterceptor.mLastIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertNotNull(contentIntent);
        return contentIntent;
    }
}
