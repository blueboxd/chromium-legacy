// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using base::UserDataAdapter;

namespace content {

// Key names on ResourceContext.
const char kURLDataManagerBackendKeyName[] = "url_data_manager_backend";

ResourceContext::ResourceContext() {}

ResourceContext::~ResourceContext() {
}

ChromeBlobStorageContext* GetChromeBlobStorageContextForResourceContext(
    const ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      resource_context, kBlobStorageContextKeyName);
}

URLDataManagerBackend* GetURLDataManagerForResourceContext(
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context->GetUserData(kURLDataManagerBackendKeyName)) {
    context->SetUserData(kURLDataManagerBackendKeyName,
                         std::make_unique<URLDataManagerBackend>());
  }
  return static_cast<URLDataManagerBackend*>(
      context->GetUserData(kURLDataManagerBackendKeyName));
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();

  resource_context->SetUserData(
      kBlobStorageContextKeyName,
      std::make_unique<UserDataAdapter<ChromeBlobStorageContext>>(
          ChromeBlobStorageContext::GetFor(browser_context)));

  resource_context->DetachFromSequence();
}

}  // namespace content
