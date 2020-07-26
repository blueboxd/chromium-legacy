// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/serial_utils.h"
#include "components/paint_preview/common/test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

TEST(PaintPreviewRecorderUtilsTest, TestParseGlyphs) {
  auto typeface = SkTypeface::MakeDefault();
  SkFont font(typeface);
  std::string unichars_1 = "abc";
  std::string unichars_2 = "efg";
  auto blob_1 = SkTextBlob::MakeFromString(unichars_1.c_str(), font);
  auto blob_2 = SkTextBlob::MakeFromString(unichars_2.c_str(), font);

  cc::PaintFlags flags;
  cc::PaintRecorder outer_recorder;
  cc::PaintCanvas* outer_canvas = outer_recorder.beginRecording(100, 100);
  outer_canvas->drawTextBlob(blob_1, 10, 10, flags);
  cc::PaintRecorder inner_recorder;
  cc::PaintCanvas* inner_canvas = inner_recorder.beginRecording(50, 50);
  inner_canvas->drawTextBlob(blob_2, 15, 20, flags);
  outer_canvas->drawPicture(inner_recorder.finishRecordingAsPicture());
  auto record = outer_recorder.finishRecordingAsPicture();

  PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                              base::UnguessableToken::Create(), true);
  ParseGlyphs(record.get(), &tracker);
  auto* usage_map = tracker.GetTypefaceUsageMap();
  EXPECT_TRUE(usage_map->count(typeface->uniqueID()));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('a')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('b')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('c')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('e')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('f')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('g')));
}

class PaintPreviewRecorderUtilsSerializeAsSkPictureTest
    : public testing::TestWithParam<mojom::Persistence> {
 public:
  PaintPreviewRecorderUtilsSerializeAsSkPictureTest()
      : tracker(base::UnguessableToken::Create(),
                base::UnguessableToken::Create(),
                true),
        dimensions(100, 100),
        recorder() {}

  ~PaintPreviewRecorderUtilsSerializeAsSkPictureTest() override = default;

 protected:
  void SetUp() override {
    canvas = recorder.beginRecording(dimensions.width(), dimensions.width());
    cc::PaintFlags flags;
    canvas->drawRect(SkRect::MakeWH(dimensions.width(), dimensions.height()),
                     flags);
  }

  base::Optional<std::unique_ptr<SkStream>> SerializeAsSkPicture(
      base::Optional<size_t> max_capture_size,
      size_t* serialized_size) {
    auto record = recorder.finishRecordingAsPicture();
    canvas = nullptr;

    switch (GetParam()) {
      case mojom::Persistence::kFileSystem: {
        base::ScopedTempDir temp_dir;
        if (!temp_dir.CreateUniqueTempDir())
          return base::nullopt;

        base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
        base::File write_file(
            file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
        if (!SerializeAsSkPictureToFile(
                record, dimensions, &tracker, std::move(write_file),
                max_capture_size.value_or(0), serialized_size))
          return base::nullopt;

        base::File read_file(file_path, base::File::FLAG_OPEN |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_EXCLUSIVE_READ);
        return {std::make_unique<FileRStream>(std::move(read_file))};
      } break;

      case mojom::Persistence::kMemoryBuffer: {
        mojo_base::BigBuffer memory_buffer;

        if (!SerializeAsSkPictureToMemoryBuffer(
                record, dimensions, &tracker, &memory_buffer,
                max_capture_size.value_or(0), serialized_size))
          return base::nullopt;

        bool copy_data = true;
        return {std::make_unique<SkMemoryStream>(
            memory_buffer.data(), memory_buffer.size(), copy_data)};
      } break;
    }

    NOTREACHED();
    return base::nullopt;
  }

  PaintPreviewTracker tracker;

  gfx::Rect dimensions;
  cc::PaintRecorder recorder;

  // Valid after SetUp() until SerializeAsSkPicture() is called.
  cc::PaintCanvas* canvas{};
};

TEST_P(PaintPreviewRecorderUtilsSerializeAsSkPictureTest, Roundtrip) {
  base::flat_set<uint32_t> ctx;
  uint32_t content_id = tracker.CreateContentForRemoteFrame(
      gfx::Rect(10, 10), base::UnguessableToken::Create());
  canvas->recordCustomData(content_id);
  ctx.insert(content_id);
  content_id = tracker.CreateContentForRemoteFrame(
      gfx::Rect(20, 20), base::UnguessableToken::Create());
  canvas->recordCustomData(content_id);
  ctx.insert(content_id);

  size_t out_size = 0;
  auto rstream = SerializeAsSkPicture(base::nullopt, &out_size);
  ASSERT_TRUE(rstream.has_value());

  SkDeserialProcs procs;
  procs.fPictureProc = [](const void* data, size_t length, void* ctx) {
    uint32_t content_id;
    if (length < sizeof(content_id))
      return MakeEmptyPicture();
    memcpy(&content_id, data, sizeof(content_id));
    auto* context = reinterpret_cast<base::flat_set<uint32_t>*>(ctx);
    EXPECT_TRUE(context->count(content_id));
    context->erase(content_id);
    return MakeEmptyPicture();
  };
  procs.fPictureCtx = &ctx;
  SkPicture::MakeFromStream(rstream.value().get(), &procs);
  EXPECT_TRUE(ctx.empty());
}

TEST_P(PaintPreviewRecorderUtilsSerializeAsSkPictureTest, FailIfExceedMaxSize) {
  size_t out_size = 2;
  auto rstream = SerializeAsSkPicture({1}, &out_size);
  EXPECT_FALSE(rstream.has_value());
  EXPECT_LE(out_size, 1U);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewRecorderUtilsSerializeAsSkPictureTest,
                         testing::Values(mojom::Persistence::kFileSystem,
                                         mojom::Persistence::kMemoryBuffer),
                         PersistenceParamToString);

TEST(PaintPreviewRecorderUtilsTest, TestBuildResponse) {
  auto token = base::UnguessableToken::Create();
  auto embedding_token = base::UnguessableToken::Create();
  PaintPreviewTracker tracker(token, embedding_token, true);
  tracker.AnnotateLink(GURL("www.google.com"), gfx::Rect(1, 2, 3, 4));
  tracker.AnnotateLink(GURL("www.chromium.org"), gfx::Rect(10, 20, 10, 20));
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 1, 1, 1),
                                      base::UnguessableToken::Create());
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 2, 4, 8),
                                      base::UnguessableToken::Create());

  auto response = mojom::PaintPreviewCaptureResponse::New();
  BuildResponse(&tracker, response.get());

  EXPECT_EQ(response->embedding_token, embedding_token);
  EXPECT_EQ(response->links.size(), 2U);
  EXPECT_THAT(response->links[0]->url, GURL("www.google.com"));
  EXPECT_THAT(response->links[0]->rect, gfx::Rect(1, 2, 3, 4));
  EXPECT_THAT(response->links[1]->url, GURL("www.chromium.org"));
  EXPECT_THAT(response->links[1]->rect, gfx::Rect(10, 20, 10, 20));

  auto* content_map = tracker.GetPictureSerializationContext();
  for (const auto& id_pair : response->content_id_to_embedding_token) {
    auto it = content_map->find(id_pair.first);
    EXPECT_NE(it, content_map->end());
    EXPECT_EQ(id_pair.first, it->first);
    EXPECT_EQ(id_pair.second, it->second);
  }
}

}  // namespace paint_preview
