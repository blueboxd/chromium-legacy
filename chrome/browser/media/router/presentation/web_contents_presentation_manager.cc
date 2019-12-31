// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/web_contents_presentation_manager.h"

#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"

namespace media_router {

// static
base::WeakPtr<WebContentsPresentationManager>
WebContentsPresentationManager::Get(content::WebContents* web_contents) {
  return PresentationServiceDelegateImpl::GetOrCreateForWebContents(
             web_contents)
      ->GetWeakPtr();
}

WebContentsPresentationManager::~WebContentsPresentationManager() = default;

}  // namespace media_router
