// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_profile_impl.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"

namespace weblayer {

namespace {

class WebResourceContext : public content::ResourceContext {
 public:
  WebResourceContext() = default;
  ~WebResourceContext() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebResourceContext);
};

}  // namespace

class WebProfileImpl::WebBrowserContext : public content::BrowserContext {
 public:
  WebBrowserContext(const base::FilePath& path) : path_(path) {
    resource_context_ = std::make_unique<WebResourceContext>();
    content::BrowserContext::Initialize(this, path_);
  }

  ~WebBrowserContext() override { NotifyWillBeDestroyed(this); }

  // BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath&) override {
    return nullptr;
  }
#endif  // !defined(OS_ANDROID)

  base::FilePath GetPath() override { return path_; }

  bool IsOffTheRecord() override { return path_.empty(); }

  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override {
    return nullptr;
  }

  content::ResourceContext* GetResourceContext() override {
    return resource_context_.get();
  }

  content::BrowserPluginGuestManager* GetGuestManager() override {
    return nullptr;
  }

  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override {
    return nullptr;
  }

  content::PushMessagingService* GetPushMessagingService() override {
    return nullptr;
  }

  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override {
    return nullptr;
  }

  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return nullptr;
  }

  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override {
    return nullptr;
  }

  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override {
    return nullptr;
  }

  content::BackgroundSyncController* GetBackgroundSyncController() override {
    return nullptr;
  }

  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override {
    return nullptr;
  }

  content::ContentIndexProvider* GetContentIndexProvider() override {
    return nullptr;
  }

 private:
  base::FilePath path_;
  std::unique_ptr<WebResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(WebBrowserContext);
};

WebProfileImpl::WebProfileImpl(const base::FilePath& path) : path_(path) {
  browser_context_ = std::make_unique<WebBrowserContext>(path_);
}

WebProfileImpl::~WebProfileImpl() = default;

content::BrowserContext* WebProfileImpl::GetBrowserContext() {
  return browser_context_.get();
}

void WebProfileImpl::ClearBrowsingData() {
  NOTIMPLEMENTED();
}

std::unique_ptr<WebProfile> WebProfile::Create(const base::FilePath& path) {
  return std::make_unique<WebProfileImpl>(path);
}

}  // namespace weblayer
