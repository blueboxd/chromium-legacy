// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/compositor_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

namespace {

void BindDiscardableSharedMemoryManagerOnIOThread(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
      std::move(receiver));
}

}  // namespace

mojo::Remote<mojom::PaintPreviewCompositorCollection>
CreateCompositorCollection() {
  auto collection = content::ServiceProcessHost::Launch<
      mojom::PaintPreviewCompositorCollection>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_PAINT_PREVIEW_COMPOSITOR_SERVICE_DISPLAY_NAME)
          .WithSandboxType(service_manager::SANDBOX_TYPE_PDF_COMPOSITOR)
          .Pass());
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      discardable_memory_manager;

  // Set up the discardable memory manager.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &BindDiscardableSharedMemoryManagerOnIOThread,
          discardable_memory_manager.InitWithNewPipeAndPassReceiver()));
  collection->SetDiscardableSharedMemoryManager(
      std::move(discardable_memory_manager));
  return collection;
}

}  // namespace paint_preview
