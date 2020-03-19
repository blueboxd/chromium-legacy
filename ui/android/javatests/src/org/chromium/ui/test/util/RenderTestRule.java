// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.text.TextUtils;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * A TestRule for creating Render Tests. An exception will be thrown after the test method completes
 * if the test fails.
 *
 * <pre>
 * {@code
 *
 * @RunWith(BaseJUnit4ClassRunner.class)
 * public class MyTest extends DummyUiActivityTestCase {
 *     // Provide RenderTestRule with the path from src/ to the golden directory.
 *     @Rule
 *     public RenderTestRule mRenderTestRule =
 *             new RenderTestRule("components/myfeature/test/data/android/render_tests");
 *
 *     @Test
 *     // The test must have the feature "RenderTest" for the bots to display renders.
 *     @Feature({"RenderTest"})
 *     public void testViewAppearance() {
 *         // Setup the UI.
 *         ...
 *
 *         // Render UI Elements.
 *         mRenderTestRule.render(bigWidgetView, "big_widget");
 *         mRenderTestRule.render(smallWidgetView, "small_widget");
 *     }
 * }
 *
 * }
 * </pre>
 */
public class RenderTestRule extends TestWatcher {
    private static final String TAG = "RenderTest";

    private static final String DIFF_FOLDER_RELATIVE = "/diffs";
    private static final String FAILURE_FOLDER_RELATIVE = "/failures";
    private static final String GOLDEN_FOLDER_RELATIVE = "/goldens";

    /**
     * This is a list of model-SDK version identifiers for devices we maintain golden images for.
     * If render tests are being run on a device of a model-sdk on this list, goldens should exist.
     * TODO(https://crbug.com/1060245): Re-add Nexus_5-19 when chrome_public_test_apk is back on the
     *      KitKat bot on the CQ.
     */
    private static final String[] RENDER_TEST_MODEL_SDK_PAIRS = {"Nexus_5X-23"};

    private enum ComparisonResult { MATCH, MISMATCH, GOLDEN_NOT_FOUND }

    // State for a test class.
    private final String mOutputFolder;
    private final String mGoldenFolder;

    // State for a test method.
    private String mTestClassName;
    private List<String> mMismatchIds = new LinkedList<>();
    private List<String> mGoldenMissingIds = new LinkedList<>();
    private boolean mHasRenderTestFeature;

    /** Parameterized tests have a prefix inserted at the front of the test description. */
    private String mVariantPrefix;

    /** Prefix on the render test images that describes light/dark mode. */
    private String mNightModePrefix;

    // How much a channel must differ when comparing pixels in order to be considered different.
    private int mPixelDiffThreshold;

    private Map<String, DiffReport> mMismatchReports = new HashMap<>();

    /**
     * An exception thrown after a Render Test if images do not match the goldens or goldens are
     * missing on a render test device.
     */
    public static class RenderTestException extends RuntimeException {
        public RenderTestException(String message) {
            super(message);
        }
    }

    public RenderTestRule(String goldenFolder) {
        // |goldenFolder| is relative to the src directory in the repository. |mGoldenFolder| will
        // be the folder on the test device.
        mGoldenFolder = UrlUtils.getIsolatedTestFilePath(goldenFolder);
        // The output folder can be overridden with the --render-test-output-dir command.
        mOutputFolder = CommandLine.getInstance().getSwitchValue("render-test-output-dir");
    }

    @Override
    protected void starting(Description desc) {
        // desc.getClassName() gets the fully qualified name.
        mTestClassName = desc.getTestClass().getSimpleName();

        mMismatchIds.clear();
        mGoldenMissingIds.clear();

        Feature feature = desc.getAnnotation(Feature.class);
        mHasRenderTestFeature =
                (feature != null && Arrays.asList(feature.value()).contains("RenderTest"));
    }

    /**
     * Renders the |view| and compares it to the golden view with the |id|. The RenderTestRule will
     * throw an exception after the test method has completed if the view does not match the
     * golden or if a golden is missing on a device it should be present (see
     * {@link RenderTestRule#RENDER_TEST_MODEL_SDK_PAIRS}).
     *
     * @throws IOException if the rendered image cannot be saved to the device.
     */
    public void render(final View view, String id) throws IOException {
        Assert.assertTrue("Render Tests must have the RenderTest feature.", mHasRenderTestFeature);

        Bitmap testBitmap = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Bitmap>() {
            @Override
            public Bitmap call() {
                int height = view.getMeasuredHeight();
                int width = view.getMeasuredWidth();
                if (height <= 0 || width <= 0) {
                    throw new IllegalStateException(
                            "Invalid view dimensions: " + width + "x" + height);
                }

                return UiUtils.generateScaledScreenshot(view, 0, Bitmap.Config.ARGB_8888);
            }
        });

        compareForResult(testBitmap, id);
    }

    /**
     * Compares the given |testBitmap| to the golden with the |id|. The RenderTestRule will throw
     * an exception after the test method has completed if the view does not match the golden or if
     * a golden is missing on a device it should be present (see
     * {@link RenderTestRule#RENDER_TEST_MODEL_SDK_PAIRS}).
     *
     * Tests should prefer {@link RenderTestRule#render(View, String) render} to this if possible.
     *
     * @throws IOException if the rendered image cannot be saved to the device.
     */
    public void compareForResult(Bitmap testBitmap, String id) throws IOException {
        Assert.assertTrue("Render Tests must have the RenderTest feature.", mHasRenderTestFeature);

        String filename = imageName(mTestClassName, mVariantPrefix, id);

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = testBitmap.getConfig();
        File goldenFile = createGoldenPath(filename);
        Bitmap goldenBitmap = BitmapFactory.decodeFile(goldenFile.getAbsolutePath(), options);

        Pair<DiffReport, Bitmap> result = compareBitmapToGolden(testBitmap, goldenBitmap, id);
        Log.i(TAG, "RenderTest %s %s", id, result.first.toString());

        // Save the result and any interesting images.
        switch (result.first.getResult()) {
            case MATCH:
                // We don't do anything with the matches.
                break;
            case GOLDEN_NOT_FOUND:
                mGoldenMissingIds.add(id);

                saveBitmap(testBitmap, createOutputPath(FAILURE_FOLDER_RELATIVE, filename));
                break;
            case MISMATCH:
                mMismatchIds.add(id);
                mMismatchReports.put(id, result.first);

                saveBitmap(testBitmap, createOutputPath(FAILURE_FOLDER_RELATIVE, filename));
                saveBitmap(goldenBitmap, createOutputPath(GOLDEN_FOLDER_RELATIVE, filename));
                saveBitmap(result.second, createOutputPath(DIFF_FOLDER_RELATIVE, filename));
                break;
        }
    }

    /**
     * Appends the DiffReports if exists.
     */
    private void maybeAppendDiffReports(StringBuilder sb) {
        for (String key : mMismatchIds) {
            if (mMismatchReports.containsKey(key)) {
                sb.append(mMismatchReports.get(key));
            }
        }
    }

    @Override
    protected void finished(Description desc) {
        if (!onRenderTestDevice() && !mGoldenMissingIds.isEmpty()) {
            Log.d(TAG, "RenderTest missing goldens, but we are not on a render test device.");
            mGoldenMissingIds.clear();
        }

        if (mGoldenMissingIds.isEmpty() && mMismatchIds.isEmpty()) {
            // Everything passed!
            return;
        }

        StringBuilder sb = new StringBuilder();
        if (!mGoldenMissingIds.isEmpty()) {
            sb.append("RenderTest Goldens missing for: ");
            sb.append(TextUtils.join(", ", mGoldenMissingIds));
            sb.append(".");
        }

        if (!mMismatchIds.isEmpty()) {
            if (sb.length() != 0) sb.append(" ");
            sb.append("RenderTest Mismatches for: ");
            sb.append(TextUtils.join(", ", mMismatchIds));
            sb.append(".");
        }

        sb.append(" See RENDER_TESTS.md for how to fix this failure.\n");
        maybeAppendDiffReports(sb);
        throw new RenderTestException(sb.toString());
    }

    /**
     * Searches the View hierarchy and modifies the Views to provide better stability in tests. For
     * example it will disable the blinking cursor in EditTexts.
     */
    public static void sanitize(View view) {
        // Add more sanitizations as we discover more flaky attributes.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                sanitize(viewGroup.getChildAt(i));
            }
        } else if (view instanceof EditText) {
            EditText editText = (EditText) view;
            editText.setCursorVisible(false);
        }
    }

    /**
     * Returns whether goldens should exist for the current device.
     */
    private static boolean onRenderTestDevice() {
        for (String model : RENDER_TEST_MODEL_SDK_PAIRS) {
            if (model.equals(modelSdkIdentifier())) return true;
        }
        return false;
    }

    /**
     * Sets a string that will be inserted at the start of the description in the golden image name.
     * This is used to create goldens for multiple different variants of the UI.
     */
    public void setVariantPrefix(String variantPrefix) {
        mVariantPrefix = variantPrefix;
    }

    /**
     * Sets a string prefix that describes the light/dark mode in the golden image name.
     */
    public void setNightModeEnabled(boolean nightModeEnabled) {
        mNightModePrefix = nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled";
    }

    /**
     * Sets the threshold that a pixel must differ by when comparing channels in order to be
     * considered different.
     */
    public void setPixelDiffThreshold(int threshold) {
        assert threshold >= 0;
        mPixelDiffThreshold = threshold;
    }

    /**
     * Creates an image name combining the image description with details about the device
     * (eg model, current orientation).
     *
     * This function must be kept in sync with |RE_RENDER_IMAGE_NAME| from
     * src/build/android/pylib/local/device/local_device_instrumentation_test_run.py.
     */
    private String imageName(String testClass, String variantPrefix, String desc) {
        if (!TextUtils.isEmpty(mNightModePrefix)) {
            desc = mNightModePrefix + "-" + desc;
        }

        if (!TextUtils.isEmpty(variantPrefix)) {
            desc = variantPrefix + "-" + desc;
        }

        return String.format(
                Locale.getDefault(), "%s.%s.%s.png", testClass, desc, modelSdkIdentifier());
    }

    /**
     * Returns a string encoding the device model and sdk. It is used to identify device goldens.
     */
    private static String modelSdkIdentifier() {
        return Build.MODEL.replace(' ', '_') + "-" + Build.VERSION.SDK_INT;
    }

    /**
     * Saves a the given |bitmap| to the |file|.
     */
    private static void saveBitmap(Bitmap bitmap, File file) throws IOException {
        FileOutputStream out = new FileOutputStream(file);
        try {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } finally {
            out.close();
        }
    }

    /**
     * Convenience method to create a File pointing to |filename| in |mGoldenFolder|.
     */
    private File createGoldenPath(String filename) throws IOException {
        return createPath(mGoldenFolder, filename);
    }

    /**
     * Convenience method to create a File pointing to |filename| in the |subfolder| in
     * |mOutputFolder|.
     */
    private File createOutputPath(String subfolder, String filename) throws IOException {
        String folder = mOutputFolder != null ? mOutputFolder : mGoldenFolder;
        return createPath(folder + subfolder, filename);
    }

    private static File createPath(String folder, String filename) throws IOException {
        File path = new File(folder);
        if (!path.exists()) {
            if (!path.mkdirs()) {
                throw new IOException("Could not create " + path.getAbsolutePath());
            }
        }
        return new File(path + "/" + filename);
    }

    /**
     * Compares two Bitmaps.
     * @return A pair of DiffReport and Bitmap. If the DiffReport.mResult is MISMATCH or MATCH,
     *         the Bitmap will be a generated pixel-by-pixel difference.
     */
    private Pair<DiffReport, Bitmap> compareBitmapToGolden(
            Bitmap render, Bitmap golden, String id) {
        if (golden == null) return Pair.create(DiffReport.newGoldenNotFoundDiffReport(), null);
        // This comparison is much, much faster than doing a pixel-by-pixel comparison, so try this
        // first and only fall back to the pixel comparison if it fails.
        if (render.sameAs(golden)) return Pair.create(DiffReport.newMatchDiffReport(), null);

        Bitmap diff = Bitmap.createBitmap(Math.max(render.getWidth(), golden.getWidth()),
                Math.max(render.getHeight(), golden.getHeight()), render.getConfig());
        // Assume that the majority of the pixels will be the same and set the diff image to
        // transparent by default.
        diff.eraseColor(Color.TRANSPARENT);

        int maxWidth = Math.max(render.getWidth(), golden.getWidth());
        int maxHeight = Math.max(render.getHeight(), golden.getHeight());
        int minWidth = Math.min(render.getWidth(), golden.getWidth());
        int minHeight = Math.min(render.getHeight(), golden.getHeight());

        DiffReport.Builder report =
                comparePixels(render, golden, diff, mPixelDiffThreshold, 0, minWidth, 0, minHeight);
        report.addPixelDiffCount(compareSizes(diff, minWidth, maxWidth, minHeight, maxHeight));
        report.setID(id);

        return Pair.create(report.build(), diff);
    }

    /**
     * The class represents a read only copy of comparison result between one test rendered image
     * and its golden image.
     */
    static class DiffReport {
        // The maximum value difference in the red channel.
        private final int mMaxRDiff;
        // The maximum value difference in the green channel.
        private final int mMaxGDiff;
        // The maximum value difference in the blue channel.
        private final int mMaxBDiff;
        // The maximum value difference in the alpha channel.
        private final int mMaxADiff;
        // Only store first 20 pixel coordinates.
        private final List<Pair<Integer, Integer>> mDiffPixelLocations = new ArrayList<>();
        // The count of pixels being considered diff.
        private final int mDiffPixelCount;
        // The comparison result such as Match, Mismatch, and Golden_not_found.
        private final ComparisonResult mResult;
        // The ID used to identify the golden image.
        private final String mID;
        // The Match DiffReport with no ID associated to it.
        private static final DiffReport MATCH_DIFF_REPORT =
                new DiffReport("", 0, 0, 0, 0, null, 0, ComparisonResult.MATCH);
        // The GoldenNotFound DiffReport with no ID associated to it.
        private static final DiffReport GOLDEN_NOT_FOUND_DIFF_REPORT =
                new DiffReport("", 0, 0, 0, 0, null, 0, ComparisonResult.GOLDEN_NOT_FOUND);
        // The maximum number of entries in mDiffPixelCount
        private static final int MAXIMUM_TOTAL_PIXEL_LOCATIONS = 20;

        private DiffReport(String id, int rDiff, int gDiff, int bDiff, int aDiff,
                List<Pair<Integer, Integer>> diffLocations, int diffPixelCount) {
            this(id, rDiff, gDiff, bDiff, aDiff, diffLocations, diffPixelCount,
                    ComparisonResult.MISMATCH);
        }

        private DiffReport(String id, int rDiff, int gDiff, int bDiff, int aDiff,
                List<Pair<Integer, Integer>> diffLocations, int diffPixelCount,
                ComparisonResult result) {
            mID = id;
            mMaxRDiff = rDiff;
            mMaxGDiff = gDiff;
            mMaxBDiff = bDiff;
            mMaxADiff = aDiff;
            if (diffLocations != null) {
                for (Pair<Integer, Integer> loc : diffLocations) {
                    mDiffPixelLocations.add(loc);
                    if (mDiffPixelLocations.size() >= MAXIMUM_TOTAL_PIXEL_LOCATIONS) {
                        break;
                    }
                }
            }
            mDiffPixelCount = diffPixelCount;
            switch (result) {
                case MISMATCH:
                    if (isMismatch()) {
                        mResult = ComparisonResult.MISMATCH;
                    } else {
                        // If the isMismatch() check fails, it is a Match.
                        mResult = ComparisonResult.MATCH;
                    }
                    break;
                case MATCH:
                case GOLDEN_NOT_FOUND:
                    mResult = result;
                    break;
                default:
                    mResult = ComparisonResult.MISMATCH;
            }
        }

        static DiffReport newMatchDiffReport() {
            return MATCH_DIFF_REPORT;
        }

        static DiffReport newGoldenNotFoundDiffReport() {
            return GOLDEN_NOT_FOUND_DIFF_REPORT;
        }

        /**
         * Retrieves a new Builder.
         */
        static DiffReport.Builder newBuilder() {
            return new Builder();
        }

        /**
         * Generates a string format of the DiffReport.
         */
        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("====DiffReport====\n");
            sb.append(String.format(Locale.getDefault(), "id : %s\n", mID));
            sb.append(String.format(Locale.getDefault(),
                    "There are %d pixels rendered differently.\n", mDiffPixelCount));
            sb.append(String.format(Locale.getDefault(),
                    "The maximum difference in RGBA channels are {R:%d, G:%d, B:%d, A:%d}\n",
                    mMaxRDiff, mMaxGDiff, mMaxBDiff, mMaxADiff));
            sb.append(String.format(Locale.getDefault(),
                    "The different pixels are located at (limited to first %d entries):\n",
                    MAXIMUM_TOTAL_PIXEL_LOCATIONS));
            for (Pair<Integer, Integer> loc : mDiffPixelLocations) {
                sb.append(String.format(Locale.getDefault(), "(%d, %d)\n", loc.first, loc.second));
            }
            sb.append("====End of DiffReport====\n");
            return sb.toString();
        }

        boolean isMismatch() {
            return mDiffPixelCount > 0;
        }

        ComparisonResult getResult() {
            return mResult;
        }

        /**
         * Allows constructing a new DiffReport with read-only properties.
         */
        static class Builder {
            private int mMaxRedDiff;
            private int mMaxGreenDiff;
            private int mMaxBlueDiff;
            private int mMaxAlphaDiff;
            private int mDiffPixelCount;
            private String mID;
            private List<Pair<Integer, Integer>> mDiffPixelLocations = new ArrayList<>();

            Builder setMaxRedDiff(int value) {
                mMaxRedDiff = value;
                return this;
            }

            Builder setMaxGreenDiff(int value) {
                mMaxGreenDiff = value;
                return this;
            }

            Builder setMaxBlueDiff(int value) {
                mMaxBlueDiff = value;
                return this;
            }

            Builder setMaxAlphaDiff(int value) {
                mMaxAlphaDiff = value;
                return this;
            }

            Builder addPixelLocation(Pair<Integer, Integer> loc) {
                mDiffPixelLocations.add(loc);
                return this;
            }

            Builder setPixelDiffCount(int count) {
                mDiffPixelCount = count;
                return this;
            }

            Builder addPixelDiffCount(int count) {
                mDiffPixelCount += count;
                return this;
            }

            Builder setID(String id) {
                mID = id;
                return this;
            }

            DiffReport build() {
                return new DiffReport(mID, mMaxRedDiff, mMaxGreenDiff, mMaxBlueDiff, mMaxAlphaDiff,
                        mDiffPixelLocations, mDiffPixelCount);
            }
        }
    }

    /**
     * Compares two bitmaps pixel-wise.
     *
     * @param testImage Bitmap of test image.
     *
     * @param goldenImage Bitmap of golden image.
     *
     * @param diffImage This is an output argument. Function will set pixels in the |diffImage| to
     * either transparent or red depending on whether that pixel differed in the golden and test
     * bitmaps. diffImage should have its width and height be the max width and height of the
     * golden and test bitmaps.
     *
     * @param diffThreshold Threshold for when to consider two color values as different. These
     * values are 8 bit (256) so this threshold value should be in range 0-256.
     *
     * @param startWidth Start x-coord to start diffing the Bitmaps.
     *
     * @param endWidth End x-coord to start diffing the Bitmaps.
     *
     * @param startHeight Start y-coord to start diffing the Bitmaps.
     *
     * @param endHeight End x-coord to start diffing the Bitmaps.
     *
     * @return Returns a DiffReport.Builder that is used to create a DiffReport.
     */
    private static DiffReport.Builder comparePixels(Bitmap testImage, Bitmap goldenImage,
            Bitmap diffImage, int diffThreshold, int startWidth, int endWidth, int startHeight,
            int endHeight) {
        int diffPixels = 0;

        // Get copies of the pixels and compare using that instead of repeatedly calling getPixel,
        // as that's significantly faster since we don't need to repeatedly hop through JNI.
        int diffWidth = endWidth - startWidth;
        int diffHeight = endHeight - startHeight;
        int[] goldenPixels =
                writeBitmapToArray(goldenImage, startWidth, startHeight, diffWidth, diffHeight);
        int[] testPixels =
                writeBitmapToArray(testImage, startWidth, startHeight, diffWidth, diffHeight);

        int maxRedDiff = 0;
        int maxGreenDiff = 0;
        int maxBlueDiff = 0;
        int maxAlphaDiff = 0;
        DiffReport.Builder builder = DiffReport.newBuilder();

        int diffArea = diffHeight * diffWidth;
        for (int i = 0; i < diffArea; ++i) {
            if (goldenPixels[i] == testPixels[i]) continue;
            int goldenColor = goldenPixels[i];
            int testColor = testPixels[i];

            int redDiff = Math.abs(Color.red(goldenColor) - Color.red(testColor));
            int greenDiff = Math.abs(Color.green(goldenColor) - Color.green(testColor));
            int blueDiff = Math.abs(Color.blue(goldenColor) - Color.blue(testColor));
            int alphaDiff = Math.abs(Color.alpha(goldenColor) - Color.alpha(testColor));

            if (redDiff > maxRedDiff) {
                maxRedDiff = redDiff;
            }
            if (blueDiff > maxBlueDiff) {
                maxBlueDiff = blueDiff;
            }
            if (greenDiff > maxGreenDiff) {
                maxGreenDiff = greenDiff;
            }
            if (alphaDiff > maxAlphaDiff) {
                maxAlphaDiff = alphaDiff;
            }

            if (redDiff > diffThreshold || blueDiff > diffThreshold || greenDiff > diffThreshold
                    || alphaDiff > diffThreshold) {
                builder.addPixelLocation(Pair.create(i % diffWidth, i / diffWidth));
                diffPixels++;
                diffImage.setPixel(i % diffWidth, i / diffWidth, Color.RED);
            }
        }
        return builder.setMaxRedDiff(maxRedDiff)
                .setMaxGreenDiff(maxGreenDiff)
                .setMaxBlueDiff(maxBlueDiff)
                .setMaxAlphaDiff(maxAlphaDiff)
                .setPixelDiffCount(diffPixels);
    }

    /**
     * Compares two bitmaps size.
     *
     * @param diffImage This is an output argument. Function will set pixels in the |diffImage| to
     * either transparent or red depending on whether that pixel coordinate occurs in the
     * dimensions of the golden and not the test bitmap or vice-versa.
     *
     * @param minWidth Min width of golden and test bitmaps.
     *
     * @param maxWidth Max width of golden and test bitmaps.
     *
     * @param minHeight Min height of golden and test bitmaps.
     *
     * @param maxHeight Max height of golden and test bitmaps.
     *
     * @return Returns number of pixels that differ between |goldenImage| and |testImage| due to
     * their size.
     */
    private static int compareSizes(
            Bitmap diffImage, int minWidth, int maxWidth, int minHeight, int maxHeight) {
        int diffPixels = 0;

        if (maxWidth > minWidth) {
            int diffWidth = maxWidth - minWidth;
            int totalPixels = diffWidth * maxHeight;
            // Filling an array of pixels then bulk-setting is faster than looping through each
            // individual pixel and setting it.
            int[] pixels = new int[totalPixels];
            Arrays.fill(pixels, 0, totalPixels, Color.RED);
            diffImage.setPixels(pixels, 0 /* offset */, diffWidth /* stride */, minWidth /* x */,
                    0 /* y */, diffWidth /* width */, maxHeight /* height */);
            diffPixels += totalPixels;
        }
        if (maxHeight > minHeight) {
            int diffHeight = maxHeight - minHeight;
            int totalPixels = diffHeight * minWidth;
            int[] pixels = new int[totalPixels];
            Arrays.fill(pixels, 0, totalPixels, Color.RED);
            diffImage.setPixels(pixels, 0 /* offset */, minWidth /* stride */, 0 /* x */,
                    minHeight /* y */, minWidth /* width */, diffHeight /* height */);
            diffPixels += totalPixels;
        }
        return diffPixels;
    }

    private static int[] writeBitmapToArray(Bitmap bitmap, int x, int y, int width, int height) {
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0 /* offset */, width /* stride */, x, y, width, height);
        return pixels;
    }
}
