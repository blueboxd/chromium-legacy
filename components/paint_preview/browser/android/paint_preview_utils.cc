// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/android/jni_headers/PaintPreviewUtils_jni.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW)

namespace {

const char kPaintPreviewTestTag[] = "PaintPreviewTest ";

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
const char kPaintPreviewDir[] = "paint_preview";
const char kCaptureTestDir[] = "capture_test";

struct CaptureMetrics {
  int compressed_size_bytes;
  int capture_time_us;
  bool success;
};

void CleanupAndLogResult(const base::FilePath& root_dir,
                         const CaptureMetrics& metrics) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DeleteFileRecursively(root_dir);
  VLOG(1) << kPaintPreviewTestTag
          << "Capture Finished: " << ((metrics.success) ? "Success" : "Failure")
          << "\n"
          << "Compressed size " << metrics.compressed_size_bytes << " bytes\n"
          << "Time taken in native " << metrics.capture_time_us << " us";
}

void MeasureSize(const base::FilePath& root_dir,
                 const GURL& url,
                 std::unique_ptr<paint_preview::PaintPreviewProto> proto,
                 CaptureMetrics metrics) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!metrics.success) {
    metrics.success = false;
    CleanupAndLogResult(root_dir, metrics);
    return;
  }

  paint_preview::FileManager manager(root_dir);
  base::FilePath path;
  bool success = manager.CreateOrGetDirectoryFor(url, &path);
  if (!success) {
    VLOG(1) << kPaintPreviewTestTag << "Failure: could not create url dir.";
    metrics.success = false;
    CleanupAndLogResult(root_dir, metrics);
    return;
  }
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  std::string str_proto = proto->SerializeAsString();
  file.WriteAtCurrentPos(str_proto.data(), str_proto.size());
  manager.CompressDirectoryFor(url);
  metrics.compressed_size_bytes = manager.GetSizeOfArtifactsFor(url);
  CleanupAndLogResult(root_dir, metrics);
  return;
}

void OnCaptured(base::TimeTicks start_time,
                const base::FilePath& root_dir,
                const GURL& url,
                base::UnguessableToken guid,
                paint_preview::mojom::PaintPreviewStatus status,
                std::unique_ptr<paint_preview::PaintPreviewProto> proto) {
  base::TimeDelta time_delta = base::TimeTicks::Now() - start_time;
  CaptureMetrics result = {
      0, time_delta.InMicroseconds(),
      status == paint_preview::mojom::PaintPreviewStatus::kOk};
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&MeasureSize, root_dir, url, std::move(proto), result));
}
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW)

}  // namespace

// If the ENABLE_PAINT_PREVIEW buildflags is set this method will trigger a
// series of actions;
// 1. Capture a paint preview via the client and measure the time taken.
// 2. Zip a folder containing the artifacts and measure the size of the zip.
// 3. Delete the resulting zip archive.
// 4. Log the results.
// If the buildflag is not set this is just a stub.
static void JNI_PaintPreviewUtils_CapturePaintPreview(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  auto* contents = content::WebContents::FromJavaWebContents(jweb_contents);
  paint_preview::PaintPreviewClient::CreateForWebContents(contents);
  auto* client = paint_preview::PaintPreviewClient::FromWebContents(contents);
  if (!client) {
    VLOG(1) << kPaintPreviewTestTag << "Failure: client could not be created.";
    return;
  }
  paint_preview::PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.is_main_frame = true;
  base::FilePath root_path = contents->GetBrowserContext()
                                 ->GetPath()
                                 .AppendASCII(kPaintPreviewDir)
                                 .AppendASCII(kCaptureTestDir);
  params.root_dir = root_path;
  GURL url = contents->GetLastCommittedURL();
  paint_preview::FileManager manager(root_path);
  bool success = manager.CreateOrGetDirectoryFor(url, &params.root_dir);
  if (!success) {
    VLOG(1) << kPaintPreviewTestTag << "Failure: could not create output dir.";
    return;
  }
  auto start_time = base::TimeTicks::Now();
  client->CapturePaintPreview(
      params, contents->GetMainFrame(),
      base::BindOnce(&OnCaptured, start_time, root_path, url));
#else
  // In theory this is unreachable as the codepath to reach here is only exposed
  // if the buildflag for ENABLE_PAINT_PREVIEW is set. However, this function
  // will still be compiled as it is called from JNI so this is here as a
  // placeholder.
  VLOG(1) << kPaintPreviewTestTag
          << "Failure: compiled without buildflag ENABLE_PAINT_PREVIEW.";
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW)
}
