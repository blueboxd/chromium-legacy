// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.shell_apk.R;
import org.chromium.webapk.shell_apk.WebApkUtils;

/** Contains splash screen related utility methods. */
public class SplashUtils {
    /** Creates view with splash screen. */
    public static View createSplashView(Context context) {
        Resources resources = context.getResources();
        Bitmap icon = WebApkUtils.decodeBitmapFromDrawable(resources, R.drawable.splash_icon);
        @SplashLayout.IconClassification
        int iconClassification = SplashLayout.classifyIcon(resources, icon, false);
        int backgroundColor = WebApkUtils.getColor(resources, R.color.background_color_non_empty);

        FrameLayout layout = new FrameLayout(context);
        SplashLayout.createLayout(context, layout, icon, false /* isIconAdaptive */,
                iconClassification, resources.getString(R.string.name),
                WebApkUtils.shouldUseLightForegroundOnBackground(backgroundColor));
        return layout;
    }

    /**
     * Returns bitmap with screenshot of passed-in view. Downsamples screenshot so that it is
     * no more than {@maxSizeInBytes}.
     */
    public static Bitmap screenshotView(View view, int maxSizeBytes) {
        // Implementation copied from Android shared element code -
        // TransitionUtils#createViewBitmap().

        int bitmapWidth = view.getWidth();
        int bitmapHeight = view.getHeight();
        if (bitmapWidth == 0 || bitmapHeight == 0) return null;

        float scale = Math.min(1f, ((float) maxSizeBytes) / (4 * bitmapWidth * bitmapHeight));
        bitmapWidth = Math.round(bitmapWidth * scale);
        bitmapHeight = Math.round(bitmapHeight * scale);

        Matrix matrix = new Matrix();
        matrix.postScale(scale, scale);

        Bitmap bitmap = Bitmap.createBitmap(bitmapWidth, bitmapHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.concat(matrix);
        view.draw(canvas);
        return bitmap;
    }

    /** Creates splash view with the passed-in dimensions and screenshots it. */
    public static Bitmap createAndImmediatelyScreenshotSplashView(
            Context context, int splashWidth, int splashHeight, int maxSizeBytes) {
        if (splashWidth <= 0 || splashHeight <= 0) return null;

        View splashView = createSplashView(context);
        splashView.measure(View.MeasureSpec.makeMeasureSpec(splashWidth, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(splashHeight, View.MeasureSpec.EXACTLY));
        splashView.layout(0, 0, splashWidth, splashHeight);
        return screenshotView(splashView, maxSizeBytes);
    }
}
