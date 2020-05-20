// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile_skia.h"

#include "cc/paint/paint_record.h"
#include "printing/common/metafile_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace printing {

TEST(MetafileSkiaTest, TestFrameContent) {
  constexpr int kPictureSideLen = 100;
  constexpr int kPageSideLen = 150;

  // Create a placeholder picture.
  sk_sp<SkPicture> pic_holder = SkPicture::MakePlaceholder(
      SkRect::MakeXYWH(0, 0, kPictureSideLen, kPictureSideLen));

  // Create the page with nested content which is the placeholder and will be
  // replaced later.
  sk_sp<cc::PaintRecord> record = sk_make_sp<cc::PaintRecord>();
  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  const SkRect page_rect = SkRect::MakeXYWH(0, 0, kPageSideLen, kPageSideLen);
  record->push<cc::DrawRectOp>(page_rect, flags);
  const uint32_t content_id = pic_holder->uniqueID();
  record->push<cc::CustomDataOp>(content_id);
  SkSize page_size = SkSize::Make(kPageSideLen, kPageSideLen);

  // Finish creating the entire metafile.
  MetafileSkia metafile(SkiaDocumentType::MSKP, 1);
  metafile.AppendPage(page_size, std::move(record));
  metafile.AppendSubframeInfo(content_id, 2, std::move(pic_holder));
  metafile.FinishFrameContent();
  SkStreamAsset* metafile_stream = metafile.GetPdfData();
  ASSERT_TRUE(metafile_stream);

  // Draw a 100 by 100 red square which will be the actual content of
  // the placeholder.
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(kPictureSideLen, kPictureSideLen);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorRED);
  paint.setAlpha(SK_AlphaOPAQUE);
  canvas->drawRect(SkRect::MakeXYWH(0, 0, kPictureSideLen, kPictureSideLen),
                   paint);
  sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());
  EXPECT_TRUE(picture);

  // Get the complete picture by replacing the placeholder.
  DeserializationContext subframes;
  subframes[content_id] = picture;
  SkDeserialProcs procs = DeserializationProcs(&subframes);
  sk_sp<SkPicture> pic = SkPicture::MakeFromStream(metafile_stream, &procs);
  ASSERT_TRUE(pic);

  // Verify the resultant picture is as expected by comparing the sizes and
  // detecting the color inside and outside of the square area.
  EXPECT_TRUE(pic->cullRect() == page_rect);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kPageSideLen, kPageSideLen);
  SkCanvas bitmap_canvas(bitmap);
  pic->playback(&bitmap_canvas);
  // Check top left pixel color of the red square.
  EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorRED);
  // Check bottom right pixel of the red square.
  EXPECT_EQ(bitmap.getColor(kPictureSideLen - 1, kPictureSideLen - 1),
            SK_ColorRED);
  // Check inside of the red square.
  EXPECT_EQ(bitmap.getColor(kPictureSideLen / 2, kPictureSideLen / 2),
            SK_ColorRED);
  // Check outside of the red square.
  EXPECT_EQ(bitmap.getColor(kPictureSideLen, kPictureSideLen), SK_ColorWHITE);
}

}  // namespace printing
