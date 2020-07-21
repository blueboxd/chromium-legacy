// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/javatests/paint_preview_test_service.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/test_paint_preview_policy.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/file_utils.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/player/android/javatests_jni_headers/PaintPreviewTestService_jni.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

using base::android::JavaParamRef;

namespace paint_preview {

const char kPaintPreviewDir[] = "paint_preview";
const char kTestDirName[] = "PaintPreviewTestService";

jlong JNI_PaintPreviewTestService_GetInstance(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_path) {
  base::FilePath file_path(base::android::ConvertJavaStringToUTF8(env, j_path));
  PaintPreviewTestService* service = new PaintPreviewTestService(file_path);
  return reinterpret_cast<intptr_t>(service);
}

PaintPreviewTestService::PaintPreviewTestService(const base::FilePath& path)
    : PaintPreviewBaseService(path,
                              kTestDirName,
                              std::make_unique<TestPaintPreviewPolicy>(),
                              false),
      test_data_dir_(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(kTestDirName)) {}

PaintPreviewTestService::~PaintPreviewTestService() = default;

jboolean PaintPreviewTestService::CreateSingleSkpForKey(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_key,
    const JavaParamRef<jstring>& j_url,
    jint j_width,
    jint j_height,
    const JavaParamRef<jintArray>& j_link_rects,
    const JavaParamRef<jobjectArray>& j_link_urls) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::PathExists(test_data_dir_)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(test_data_dir_, &error)) {
      LOG(ERROR) << "Failed to create dir: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }

  base::FilePath path = test_data_dir_.AppendASCII(
      base::android::ConvertJavaStringToUTF8(env, j_key));
  if (!base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create directory.";
    return false;
  }

  uint32_t width = static_cast<uint32_t>(j_width);
  uint32_t height = static_cast<uint32_t>(j_height);

  SkPictureRecorder recorder;
  auto* canvas = recorder.beginRecording(SkRect::MakeWH(width, height));
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  canvas->drawRect(SkRect::MakeWH(width, height), paint);
  paint.setColor(SK_ColorGRAY);
  const int kSquareSideLen = 50;
  for (uint32_t j = 0; j * kSquareSideLen < height; ++j) {
    for (uint32_t i = (j % 2); i * kSquareSideLen < width; i += 2) {
      canvas->drawRect(SkRect::MakeXYWH(i * kSquareSideLen, j * kSquareSideLen,
                                        kSquareSideLen, kSquareSideLen),
                       paint);
    }
  }
  auto skp = recorder.finishRecordingAsPicture();
  auto skp_path = path.AppendASCII("test_file.skp");
  FileWStream wstream(base::File(
      skp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
  skp->serialize(&wstream);
  wstream.Close();
  if (wstream.DidWriteFail()) {
    LOG(ERROR) << "SKP Write failed";
    return false;
  }
  LOG(INFO) << "Wrote SKP " << wstream.ActualBytesWritten() << " bytes";

  PaintPreviewProto paint_preview;
  auto* metadata = paint_preview.mutable_metadata();
  metadata->set_url(base::android::ConvertJavaStringToUTF8(env, j_url));

  auto* root_frame = paint_preview.mutable_root_frame();
  auto token = base::UnguessableToken::Create();
  root_frame->set_file_path(skp_path.AsUTF8Unsafe());
  root_frame->set_embedding_token_low(token.GetLowForSerialization());
  root_frame->set_embedding_token_high(token.GetHighForSerialization());
  root_frame->set_is_main_frame(true);
  // No initial offset.
  root_frame->set_scroll_offset_x(0);
  root_frame->set_scroll_offset_y(0);

  std::vector<std::string> link_urls;
  base::android::AppendJavaStringArrayToStringVector(env, j_link_urls,
                                                     &link_urls);
  std::vector<int> link_rects;
  base::android::JavaIntArrayToIntVector(env, j_link_rects, &link_rects);
  for (size_t i = 0; i < link_urls.size(); ++i) {
    auto* link_proto = root_frame->add_links();
    link_proto->set_url(link_urls[i]);
    auto* rect = link_proto->mutable_rect();
    rect->set_x(link_rects[i * 4]);
    rect->set_y(link_rects[i * 4 + 1]);
    rect->set_width(link_rects[i * 4 + 2]);
    rect->set_height(link_rects[i * 4 + 3]);
  }
  if (!WriteProtoToFile(path.AppendASCII("proto.pb"), paint_preview)) {
    LOG(ERROR) << "Failed to write proto to file.";
    return false;
  }
  return true;
}

}  // namespace paint_preview
