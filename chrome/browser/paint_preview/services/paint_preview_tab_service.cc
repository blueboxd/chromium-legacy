// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/paint_preview/browser/file_manager.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

PaintPreviewTabService::PaintPreviewTabService(
    const base::FilePath& profile_dir,
    base::StringPiece ascii_feature_name,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(profile_dir,
                              ascii_feature_name,
                              std::move(policy),
                              is_off_the_record) {}

PaintPreviewTabService::~PaintPreviewTabService() = default;

void PaintPreviewTabService::CaptureTab(int tab_id,
                                        content::WebContents* contents,
                                        FinishedCallback callback) {
  auto file_manager = GetFileManager();
  auto key = file_manager->CreateKey(tab_id);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, GetFileManager(), key,
                     true),
      base::BindOnce(&PaintPreviewTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(), key,
                     base::Unretained(contents), std::move(callback)));
}

void PaintPreviewTabService::TabClosed(int tab_id) {
  auto file_manager = GetFileManager();
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteArtifactSet, file_manager,
                                file_manager->CreateKey(tab_id)));
}

void PaintPreviewTabService::AuditArtifacts(
    const std::vector<int>& active_tab_ids) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&FileManager::ListUsedKeys, GetFileManager()),
      base::BindOnce(&PaintPreviewTabService::RunAudit,
                     weak_ptr_factory_.GetWeakPtr(), active_tab_ids));
}

void PaintPreviewTabService::CaptureTabInternal(
    const DirectoryKey& key,
    content::WebContents* contents,
    FinishedCallback callback,
    const base::Optional<base::FilePath>& file_path) {
  if (!file_path.has_value()) {
    std::move(callback).Run(Status::kDirectoryCreationFailed);
    return;
  }
  CapturePaintPreview(
      contents, file_path.value(), gfx::Rect(0, 0, 0, 0),
      base::BindOnce(&PaintPreviewTabService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), key, std::move(callback)));
}

void PaintPreviewTabService::OnCaptured(
    const DirectoryKey& key,
    FinishedCallback callback,
    PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (status != PaintPreviewBaseService::CaptureStatus::kOk || !proto) {
    std::move(callback).Run(Status::kCaptureFailed);
    return;
  }
  auto file_manager = GetFileManager();
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::SerializePaintPreviewProto, GetFileManager(),
                     key, *proto, true),
      base::BindOnce(&PaintPreviewTabService::OnFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaintPreviewTabService::OnFinished(FinishedCallback callback,
                                        bool success) {
  std::move(callback).Run(success ? Status::kOk
                                  : Status::kProtoSerializationFailed);
}

void PaintPreviewTabService::RunAudit(
    const std::vector<int>& active_tab_ids,
    const base::flat_set<DirectoryKey>& in_use_keys) {
  auto file_manager = GetFileManager();
  std::vector<DirectoryKey> keys;
  keys.reserve(active_tab_ids.size());
  for (const auto& tab_id : active_tab_ids)
    keys.push_back(file_manager->CreateKey(tab_id));
  base::flat_set<DirectoryKey> active_tab_keys(std::move(keys));

  std::vector<DirectoryKey> keys_to_delete(active_tab_keys.size() +
                                           in_use_keys.size());
  auto it = std::set_difference(in_use_keys.begin(), in_use_keys.end(),
                                active_tab_keys.begin(), active_tab_keys.end(),
                                keys_to_delete.begin());
  keys_to_delete.resize(it - keys_to_delete.begin());

  GetTaskRunner()->PostTask(FROM_HERE,
                            base::BindOnce(&FileManager::DeleteArtifactSets,
                                           GetFileManager(), keys_to_delete));
}

}  // namespace paint_preview
