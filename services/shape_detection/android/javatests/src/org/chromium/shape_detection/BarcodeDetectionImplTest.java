// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.BarcodeDetectionResult;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;
import org.chromium.shape_detection.mojom.BarcodeFormat;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for BarcodeDetectionImpl.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BarcodeDetectionImplTest {
    private static final org.chromium.skia.mojom.Bitmap QR_CODE_BITMAP =
            TestUtils.mojoBitmapFromFile("qr_code.png");

    private static final int[] SUPPORTED_FORMATS = {BarcodeFormat.AZTEC, BarcodeFormat.CODE_128,
            BarcodeFormat.CODE_39, BarcodeFormat.CODE_93, BarcodeFormat.CODABAR,
            BarcodeFormat.DATA_MATRIX, BarcodeFormat.EAN_13, BarcodeFormat.EAN_8, BarcodeFormat.ITF,
            BarcodeFormat.PDF417, BarcodeFormat.QR_CODE, BarcodeFormat.UPC_A, BarcodeFormat.UPC_E};

    private static int[] enumerateSupportedFormats() {
        BarcodeDetectionProvider provider = new BarcodeDetectionProviderImpl();

        final ArrayBlockingQueue<int[]> queue = new ArrayBlockingQueue<>(13);
        provider.enumerateSupportedFormats(
                new BarcodeDetectionProvider.EnumerateSupportedFormatsResponse() {
                    @Override
                    public void call(int[] results) {
                        queue.add(results);
                    }
                });
        int[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Could not get int[] supported formats: " + e.toString());
        }
        Assert.assertNotNull(toReturn);
        return toReturn;
    }

    private static BarcodeDetectionResult[] detect(org.chromium.skia.mojom.Bitmap mojoBitmap) {
        BarcodeDetectorOptions options = new BarcodeDetectorOptions();
        return detectWithOptions(mojoBitmap, options);
    }

    private static BarcodeDetectionResult[] detectWithHint(
            org.chromium.skia.mojom.Bitmap mojoBitmap, int format) {
        Assert.assertTrue(BarcodeFormat.isKnownValue(format));
        BarcodeDetectorOptions options = new BarcodeDetectorOptions();
        options.formats = new int[] {format};
        return detectWithOptions(mojoBitmap, options);
    }

    private static BarcodeDetectionResult[] detectWithOptions(
            org.chromium.skia.mojom.Bitmap mojoBitmap, BarcodeDetectorOptions options) {
        BarcodeDetection detector = new BarcodeDetectionImpl(options);

        final ArrayBlockingQueue<BarcodeDetectionResult[]> queue = new ArrayBlockingQueue<>(1);
        detector.detect(mojoBitmap, new BarcodeDetection.DetectResponse() {
            @Override
            public void call(BarcodeDetectionResult[] results) {
                queue.add(results);
            }
        });
        BarcodeDetectionResult[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Could not get BarcodeDetectionResult: " + e.toString());
        }
        Assert.assertNotNull(toReturn);
        return toReturn;
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testEnumerateSupportedFormats() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        int[] results = enumerateSupportedFormats();
        Assert.assertArrayEquals(SUPPORTED_FORMATS, results);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectQrCodeWithHint() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        BarcodeDetectionResult[] results = detectWithHint(QR_CODE_BITMAP, BarcodeFormat.QR_CODE);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals("https://chromium.org", results[0].rawValue);
        Assert.assertEquals(40.0, results[0].boundingBox.x, 0.0);
        Assert.assertEquals(40.0, results[0].boundingBox.y, 0.0);
        Assert.assertEquals(250.0, results[0].boundingBox.width, 0.0);
        Assert.assertEquals(250.0, results[0].boundingBox.height, 0.0);
        Assert.assertEquals(BarcodeFormat.QR_CODE, results[0].format);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectQrCodeWithoutHint() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        BarcodeDetectionResult[] results = detect(QR_CODE_BITMAP);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals("https://chromium.org", results[0].rawValue);
        Assert.assertEquals(40.0, results[0].boundingBox.x, 0.0);
        Assert.assertEquals(40.0, results[0].boundingBox.y, 0.0);
        Assert.assertEquals(250.0, results[0].boundingBox.width, 0.0);
        Assert.assertEquals(250.0, results[0].boundingBox.height, 0.0);
        Assert.assertEquals(BarcodeFormat.QR_CODE, results[0].format);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testTryDetectQrCodeWithAztecHint() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        BarcodeDetectionResult[] results = detectWithHint(QR_CODE_BITMAP, BarcodeFormat.AZTEC);
        Assert.assertEquals(0, results.length);
    }
}
