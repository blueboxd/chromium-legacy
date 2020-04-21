// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/android/paint_preview_utils.h"

#include <jni.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/android/jni_headers/PaintPreviewUtils_jni.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace paint_preview {

namespace {

const char kPaintPreviewTestTag[] = "PaintPreviewTest ";
const char kPaintPreviewDir[] = "paint_preview";
const char kCaptureTestDir[] = "capture_test";

struct CaptureMetrics {
  base::TimeDelta capture_time;
  ukm::SourceId source_id;
};

void CleanupOnFailure(const base::FilePath& root_dir,
                      FinishedCallback finished) {
  VLOG(1) << kPaintPreviewTestTag << "Capture Failed\n";
  base::DeleteFileRecursively(root_dir);
  std::move(finished).Run(base::nullopt);
}

void CleanupAndLogResult(const base::FilePath& zip_path,
                         const CaptureMetrics& metrics,
                         FinishedCallback finished,
                         bool keep_zip,
                         size_t compressed_size_bytes) {
  VLOG(1) << kPaintPreviewTestTag << "Capture Finished Successfully:\n"
          << "Compressed size " << compressed_size_bytes << " bytes\n"
          << "Time taken in native " << metrics.capture_time.InMilliseconds()
          << " ms";

  if (!keep_zip)
    base::DeleteFileRecursively(zip_path.DirName());

  base::UmaHistogramMemoryKB(
      "Browser.PaintPreview.CaptureExperiment.CompressedOnDiskSize",
      compressed_size_bytes / 1000);
  if (metrics.source_id != ukm::kInvalidSourceId) {
    ukm::builders::PaintPreviewCapture(metrics.source_id)
        .SetCompressedOnDiskSize(
            ukm::GetExponentialBucketMinForBytes(compressed_size_bytes))
        .Record(ukm::UkmRecorder::Get());
  }
  std::move(finished).Run(zip_path);
}

void MeasureSize(scoped_refptr<FileManager> manager,
                 const DirectoryKey& key,
                 const base::FilePath& root_dir,
                 CaptureMetrics metrics,
                 FinishedCallback finished,
                 bool keep_zip,
                 bool success) {
  if (!success) {
    CleanupOnFailure(root_dir, std::move(finished));
    return;
  }
  manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&FileManager::GetSizeOfArtifacts, manager, key),
      base::BindOnce(
          &CleanupAndLogResult,
          root_dir.AppendASCII(key.AsciiDirname()).AddExtensionASCII("zip"),
          metrics, std::move(finished), keep_zip));
}

void OnCaptured(scoped_refptr<FileManager> manager,
                const DirectoryKey& key,
                base::TimeTicks start_time,
                const base::FilePath& root_dir,
                ukm::SourceId source_id,
                FinishedCallback finished,
                bool keep_zip,
                base::UnguessableToken guid,
                mojom::PaintPreviewStatus status,
                std::unique_ptr<PaintPreviewProto> proto) {
  base::TimeDelta time_delta = base::TimeTicks::Now() - start_time;

  bool success = (status == mojom::PaintPreviewStatus::kOk);
  base::UmaHistogramBoolean("Browser.PaintPreview.CaptureExperiment.Success",
                            success);
  if (!success || !proto) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CleanupOnFailure, root_dir, std::move(finished)));
    return;
  }

  CaptureMetrics result = {time_delta, source_id};
  manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::SerializePaintPreviewProto, manager, key,
                     *proto, true),
      base::BindOnce(&MeasureSize, manager, key, root_dir, result,
                     std::move(finished), keep_zip));
}

void InitiateCapture(scoped_refptr<FileManager> manager,
                     const DirectoryKey& key,
                     int frame_tree_node_id,
                     FinishedCallback finished,
                     bool keep_zip,
                     const base::Optional<base::FilePath>& url_path) {
  auto* contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!url_path.has_value() || !contents) {
    std::move(finished).Run(base::nullopt);
    return;
  }

  auto* client = PaintPreviewClient::FromWebContents(contents);
  if (!client) {
    VLOG(1) << kPaintPreviewTestTag << "Failure: client could not be created.";
    CleanupOnFailure(url_path->DirName(), std::move(finished));
    return;
  }

  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.is_main_frame = true;
  params.root_dir = url_path.value();
  params.max_per_capture_size = 0;  // Unlimited size.

  ukm::SourceId source_id = ukm::GetSourceIdForWebContentsDocument(contents);
  auto start_time = base::TimeTicks::Now();
  client->CapturePaintPreview(
      params, contents->GetMainFrame(),
      base::BindOnce(&OnCaptured, manager, key, start_time,
                     params.root_dir.DirName(), source_id, std::move(finished),
                     keep_zip));
}

}  // namespace

void Capture(content::WebContents* contents,
             FinishedCallback finished,
             bool keep_zip) {
  PaintPreviewClient::CreateForWebContents(contents);
  base::FilePath root_path = contents->GetBrowserContext()
                                 ->GetPath()
                                 .AppendASCII(kPaintPreviewDir)
                                 .AppendASCII(kCaptureTestDir);
  auto manager = base::MakeRefCounted<FileManager>(
      root_path, base::ThreadPool::CreateSequencedTaskRunner(
                     {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                      base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                      base::ThreadPolicy::MUST_USE_FOREGROUND}));

  auto key = manager->CreateKey(contents->GetLastCommittedURL());
  manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, manager, key, true),
      base::BindOnce(&InitiateCapture, manager, key,
                     contents->GetMainFrame()->GetFrameTreeNodeId(),
                     std::move(finished), keep_zip));
}

}  // namespace paint_preview

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
  paint_preview::Capture(contents, base::DoNothing(), /* keep_zip= */ false);
#else
  // In theory this is unreachable as the codepath to reach here is only exposed
  // if the buildflag for ENABLE_PAINT_PREVIEW is set. However, this function
  // will still be compiled as it is called from JNI so this is here as a
  // placeholder.
  VLOG(1) << paint_preview::kPaintPreviewTestTag
          << "Failure: compiled without buildflag ENABLE_PAINT_PREVIEW.";
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW)
}
