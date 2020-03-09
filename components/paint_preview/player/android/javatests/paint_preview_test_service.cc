// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/javatests/paint_preview_test_service.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/test_paint_preview_policy.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/player/android/javatests_jni_headers/PaintPreviewTestService_jni.h"

using base::android::JavaParamRef;

namespace paint_preview {

namespace {
const char kPaintPreviewDir[] = "paint_preview";
const char kTestDirName[] = "PaintPreviewTestService";

void UpdateSkpPaths(const base::FilePath& test_data_dir,
                    const DirectoryKey& key,
                    PaintPreviewBaseService::OnReadProtoCallback callback,
                    std::unique_ptr<PaintPreviewProto> proto) {
  // Update the file path for the root SKP to match the isolated test
  // environment.
  std::string root_skp_file_name =
      base::FilePath(proto->root_frame().file_path()).BaseName().AsUTF8Unsafe();
  base::FilePath root_skp_file_path =
      test_data_dir.AppendASCII(key.AsciiDirname())
          .AppendASCII(root_skp_file_name);
  proto->mutable_root_frame()->set_file_path(root_skp_file_path.AsUTF8Unsafe());

  // Update the file path for the subframe SKPs to match the isolated test
  // environment.
  for (auto& subframe : *(proto->mutable_subframes())) {
    std::string subframe_skp_file_name =
        base::FilePath(subframe.file_path()).BaseName().AsUTF8Unsafe();
    base::FilePath subframe_skp_file_path =
        test_data_dir.AppendASCII(key.AsciiDirname())
            .AppendASCII(subframe_skp_file_name);
    subframe.set_file_path(subframe_skp_file_path.AsUTF8Unsafe());
  }
  std::move(callback).Run(std::move(proto));
}
}  // namespace

jlong JNI_PaintPreviewTestService_GetInstance(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_test_data_dir) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, j_test_data_dir));
  PaintPreviewTestService* service = new PaintPreviewTestService(file_path);
  return reinterpret_cast<intptr_t>(service);
}

PaintPreviewTestService::PaintPreviewTestService(
    const base::FilePath& test_data_dir)
    : PaintPreviewBaseService(test_data_dir,
                              kTestDirName,
                              std::make_unique<TestPaintPreviewPolicy>(),
                              false),
      test_data_dir_(test_data_dir.AppendASCII(kPaintPreviewDir)
                         .AppendASCII(kTestDirName)) {}

PaintPreviewTestService::~PaintPreviewTestService() = default;

void PaintPreviewTestService::GetCapturedPaintPreviewProto(
    const DirectoryKey& key,
    OnReadProtoCallback on_read_proto_callback) {
  PaintPreviewBaseService::GetCapturedPaintPreviewProto(
      key, base::BindOnce(&UpdateSkpPaths, test_data_dir_, key,
                          std::move(on_read_proto_callback)));
}

}  // namespace paint_preview
