// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RESOURCE_LOAD_OBSERVER_H_
#define CONTENT_TEST_RESOURCE_LOAD_OBSERVER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

// Observer class to track resource loads.
class ResourceLoadObserver : public WebContentsObserver {
 public:
  explicit ResourceLoadObserver(Shell* shell);

  ~ResourceLoadObserver() override;

  const std::vector<mojom::ResourceLoadInfoPtr>& resource_load_infos() const {
    return resource_load_infos_;
  }

  const std::vector<bool>& resource_is_associated_with_main_frame() const {
    return resource_is_associated_with_main_frame_;
  }

  const std::vector<GURL>& memory_cached_loaded_urls() const {
    return memory_cached_loaded_urls_;
  }

  // Use this method with the SCOPED_TRACE macro, so it shows the caller context
  // if it fails.
  void CheckResourceLoaded(
      const GURL& url,
      const GURL& referrer,
      const std::string& load_method,
      content::ResourceType resource_type,
      const base::FilePath::StringPieceType& served_file_name,
      const std::string& mime_type,
      const std::string& ip_address,
      bool was_cached,
      bool first_network_request,
      const base::TimeTicks& before_request,
      const base::TimeTicks& after_request);

  // Returns the resource with the given url if found, otherwise nullptr.
  mojom::ResourceLoadInfoPtr* FindResource(const GURL& url);

  void Reset();

  void WaitForResourceCompletion(const GURL& url);

 private:
  // WebContentsObserver implementation:
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const mojom::ResourceLoadInfo& resource_load_info) override;
  void DidLoadResourceFromMemoryCache(const GURL& url,
                                      const std::string& mime_type,
                                      ResourceType resource_type) override;

  std::vector<GURL> memory_cached_loaded_urls_;
  std::vector<mojom::ResourceLoadInfoPtr> resource_load_infos_;
  std::vector<bool> resource_is_associated_with_main_frame_;
  GURL waiting_url_;
  base::OnceClosure waiting_callback_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadObserver);
};

}  // namespace content

#endif  // CONTENT_TEST_RESOURCE_LOAD_OBSERVER_H_