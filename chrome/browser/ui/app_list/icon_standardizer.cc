// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/icon_standardizer.h"

#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

constexpr float kCircleOutlineStrokeWidth = 6.0f;

constexpr int kMinimumVisibleAlpha = 40;

constexpr float kCircleShapePixelDifferenceThreshold = 0.01f;

constexpr float kInsideCircleDifferenceThreshold = 0.005f;

constexpr float kIconScaleToFit = 0.9f;

// Returns the bounding rect for the opaque part of the icon.
gfx::Rect GetVisibleIconBounds(const SkBitmap& bitmap) {
  const SkPixmap pixmap = bitmap.pixmap();

  bool const nativeColorType = pixmap.colorType() == kN32_SkColorType;

  const int width = pixmap.width();
  const int height = pixmap.height();

  // Overall bounds of the visible icon.
  int y_from = -1;
  int y_to = -1;
  int x_left = width;
  int x_right = -1;

  // Find bounding rect of the visible icon by going through all pixels one row
  // at a time and for each row find the first and the last non-transparent
  // pixel.
  for (int y = 0; y < height; y++) {
    const SkColor* nativeRow =
        nativeColorType
            ? reinterpret_cast<const SkColor*>(bitmap.getAddr32(0, y))
            : nullptr;
    bool does_row_have_visible_pixels = false;

    for (int x = 0; x < width; x++) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMinimumVisibleAlpha) {
        does_row_have_visible_pixels = true;
        x_left = std::min(x_left, x);
        break;
      }
    }

    // No visible pixels on this row.
    if (!does_row_have_visible_pixels)
      continue;

    for (int x = width - 1; x > 0; x--) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMinimumVisibleAlpha) {
        x_right = std::max(x_right, x);
        break;
      }
    }

    y_to = y;
    if (y_from == -1)
      y_from = y;
  }

  int visible_width = x_right - x_left + 1;
  int visible_height = y_to - y_from + 1;
  return gfx::Rect(x_left, y_from, visible_width, visible_height);
}

// Returns whether the shape of the icon is roughly circle shaped.
bool IsIconCircleShaped(const gfx::ImageSkia& image) {
  for (gfx::ImageSkiaRep rep : image.image_reps()) {
    if (rep.scale() != image.GetMaxSupportedScale())
      continue;

    SkBitmap bitmap(rep.GetBitmap());

    int width = bitmap.width();
    int height = bitmap.height();

    SkBitmap preview;
    preview.allocN32Pixels(width, height);
    preview.eraseColor(SK_ColorTRANSPARENT);

    // |preview| will be the original icon with all visible pixels colored red.
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        const SkColor* src_color =
            reinterpret_cast<SkColor*>(bitmap.getAddr32(0, y));
        SkColor* preview_color =
            reinterpret_cast<SkColor*>(preview.getAddr32(0, y));

        SkColor target_color;

        if (SkColorGetA(src_color[x]) < 1) {
          target_color = SK_ColorTRANSPARENT;
        } else {
          target_color = SK_ColorRED;
        }

        preview_color[x] = target_color;
      }
    }

    // Use a canvas to perform XOR and DST_OUT operations, which should
    // generate a transparent bitmap for |preview| if the original icon is
    // shaped like a circle.
    SkCanvas canvas(preview);
    SkPaint paint_circle_mask;
    paint_circle_mask.setColor(SK_ColorBLUE);
    paint_circle_mask.setStyle(SkPaint::kFill_Style);
    paint_circle_mask.setAntiAlias(true);

    // XOR operation to remove a circle.
    paint_circle_mask.setBlendMode(SkBlendMode::kXor);
    canvas.drawCircle(SkPoint::Make(width / 2, height / 2), width / 2,
                      paint_circle_mask);

    SkPaint paint_outline;
    paint_outline.setColor(SK_ColorBLUE);
    paint_outline.setStyle(SkPaint::kStroke_Style);
    paint_outline.setStrokeWidth(kCircleOutlineStrokeWidth * rep.scale());
    paint_outline.setAntiAlias(true);

    // DST_OUT operation to remove an extra circle outline.
    paint_outline.setBlendMode(SkBlendMode::kDstOut);
    canvas.drawCircle(SkPoint::Make((width - 1) / 2.0f, (height - 1) / 2.0f),
                      (width - kCircleOutlineStrokeWidth) / 2.0f,
                      paint_outline);

    // Compute the total pixel difference between the circle mask and the
    // original icon.
    int total_pixel_difference = 0;
    for (int y = 0; y < preview.height(); ++y) {
      SkColor* src_color = reinterpret_cast<SkColor*>(preview.getAddr32(0, y));
      for (int x = 0; x < preview.width(); ++x) {
        if (SkColorGetA(src_color[x]) >= kMinimumVisibleAlpha)
          total_pixel_difference++;
      }
    }

    float percentage_diff_pixels =
        static_cast<float>(total_pixel_difference) / (width * height);

    // If the pixel difference between a circle and the original icon is small
    // enough, then the icon can be considered circle shaped.
    bool is_icon_already_circle_shaped =
        !(percentage_diff_pixels >= kCircleShapePixelDifferenceThreshold);

    return is_icon_already_circle_shaped;
  }

  return false;
}

// Returns whether the opaque part of the icon can fit within a circle.
bool CanVisibleIconFitInCircle(const gfx::ImageSkia& image) {
  for (gfx::ImageSkiaRep rep : image.image_reps()) {
    if (rep.scale() != image.GetMaxSupportedScale())
      continue;

    SkBitmap bitmap(rep.GetBitmap());

    int width = bitmap.width();
    int height = bitmap.height();

    SkBitmap preview;
    preview.allocN32Pixels(width, height);
    preview.eraseColor(SK_ColorTRANSPARENT);

    // |preview| will be the original icon with all visible pixels colored red.
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        const SkColor* src_color =
            reinterpret_cast<SkColor*>(bitmap.getAddr32(0, y));
        SkColor* preview_color =
            reinterpret_cast<SkColor*>(preview.getAddr32(0, y));

        SkColor target_color;

        if (SkColorGetA(src_color[x]) < 1) {
          target_color = SK_ColorTRANSPARENT;
        } else {
          target_color = SK_ColorRED;
        }

        preview_color[x] = target_color;
      }
    }

    SkCanvas canvas(preview);
    SkPaint paint_circle_mask;
    paint_circle_mask.setColor(SK_ColorBLUE);
    paint_circle_mask.setStyle(SkPaint::kFill_Style);
    paint_circle_mask.setAntiAlias(true);

    // DST_OUT operation will generate a transparent bitmap for |preview| if the
    // original icon fits inside of a circle.
    paint_circle_mask.setBlendMode(SkBlendMode::kDstOut);
    canvas.drawCircle(SkPoint::Make(width / 2, height / 2), width / 2,
                      paint_circle_mask);

    int total_pixel_difference = 0;

    // Compute the total pixel difference between the circle mask and the
    // original icon.
    for (int y = 0; y < preview.height(); ++y) {
      SkColor* src_color = reinterpret_cast<SkColor*>(preview.getAddr32(0, y));
      for (int x = 0; x < preview.width(); ++x) {
        if (SkColorGetA(src_color[x]) >= kMinimumVisibleAlpha)
          total_pixel_difference++;
      }
    }

    float percentage_diff_pixels =
        static_cast<float>(total_pixel_difference) / (width * height);

    // If the pixel difference between a circle and the original icon is small
    // enough, then the icon can be considered as fitting inside the circle.
    return !(percentage_diff_pixels >= kInsideCircleDifferenceThreshold);
  }

  return false;
}

}  // namespace

gfx::ImageSkia CreateStandardIconImage(const gfx::ImageSkia& image) {
  gfx::ImageSkia final_image;

  // If icon is already circle shaped, then return the original image.
  if (IsIconCircleShaped(image))
    return image;

  bool visible_icon_fits_in_circle = CanVisibleIconFitInCircle(image);

  for (gfx::ImageSkiaRep rep : image.image_reps()) {
    SkBitmap unscaled_bitmap(rep.GetBitmap());
    int width = unscaled_bitmap.width();
    int height = unscaled_bitmap.height();

    SkBitmap final_bitmap;
    final_bitmap.allocN32Pixels(width, height);
    final_bitmap.eraseColor(SK_ColorTRANSPARENT);

    // To draw to |final_bitmap|, create a canvas and draw a circle background
    // with an app icon on top;
    SkCanvas canvas(final_bitmap);
    SkPaint paint_background_circle;
    paint_background_circle.setAntiAlias(true);
    paint_background_circle.setColor(SK_ColorWHITE);
    paint_background_circle.setStyle(SkPaint::kFill_Style);

    float circle_diameter = width;

    // Draw the background circle.
    canvas.drawCircle(SkPoint::Make((width - 1) / 2.0f, (height - 1) / 2.0f),
                      circle_diameter / 2.0f - 1, paint_background_circle);

    gfx::Rect visible_icon_rect = GetVisibleIconBounds(unscaled_bitmap);
    float visible_icon_diagonal =
        std::sqrt(visible_icon_rect.height() * visible_icon_rect.height() +
                  visible_icon_rect.width() * visible_icon_rect.width());

    // Calculate the icon scale required to fit the bounds of the visible icon
    // in the background circle.
    float icon_scale =
        fmin(1.0f, circle_diameter * kIconScaleToFit / visible_icon_diagonal);

    // Special case scaling when the unscaled visible icon already fits within
    // the background circle.
    if (visible_icon_fits_in_circle)
      icon_scale = kIconScaleToFit;

    SkPaint paint_icon;
    paint_icon.setMaskFilter(nullptr);
    paint_icon.setBlendMode(SkBlendMode::kSrcOver);

    if (icon_scale == 1.0f) {
      // Draw the unscaled icon on top of the background.
      canvas.drawBitmap(unscaled_bitmap, 0, 0, &paint_icon);
    } else {
      gfx::Size scaled_icon_size =
          gfx::ScaleToRoundedSize(rep.pixel_size(), icon_scale);
      const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
          unscaled_bitmap, skia::ImageOperations::RESIZE_BEST,
          scaled_icon_size.width(), scaled_icon_size.height());

      int target_left = (width - scaled_icon_size.width()) / 2;
      int target_top = (height - scaled_icon_size.height()) / 2;

      // Draw the scaled icon on top of the background.
      canvas.drawBitmap(scaled_bitmap, target_left, target_top, &paint_icon);
    }

    final_image.AddRepresentation(gfx::ImageSkiaRep(final_bitmap, rep.scale()));
  }

  return final_image;
}

}  // namespace app_list
