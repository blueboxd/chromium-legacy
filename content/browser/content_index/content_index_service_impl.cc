// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/origin.h"

namespace content {

namespace {

void CreateOnIO(blink::mojom::ContentIndexServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::MakeStrongBinding(std::make_unique<ContentIndexServiceImpl>(),
                          std::move(request));
}

}  // namespace

// static
void ContentIndexServiceImpl::Create(
    blink::mojom::ContentIndexServiceRequest request,
    RenderProcessHost* render_process_host,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&CreateOnIO, std::move(request)));
}

ContentIndexServiceImpl::ContentIndexServiceImpl() = default;

ContentIndexServiceImpl::~ContentIndexServiceImpl() = default;

void ContentIndexServiceImpl::Add(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const SkBitmap& icon,
    AddCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/973844): Implement this.
  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);
}

void ContentIndexServiceImpl::Delete(int64_t service_worker_registration_id,
                                     const std::string& content_id,
                                     DeleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/973844): Implement this.
  std::move(callback).Run(blink::mojom::ContentIndexError::NONE);
}

void ContentIndexServiceImpl::GetDescriptions(
    int64_t service_worker_registration_id,
    GetDescriptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/973844): Implement this.
  std::move(callback).Run(blink::mojom::ContentIndexError::NONE,
                          /* descriptions= */ {});
}

}  // namespace content
