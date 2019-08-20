// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"

class GURL;
class Profile;

namespace url {
class Origin;
}

namespace predictors {
class PreconnectManager;
}

namespace content_settings {
class CookieSettings;
}

namespace network_hints {
struct LookupRequest;
}

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class ChromeRenderMessageFilter : public content::BrowserMessageFilter {
 public:
  ChromeRenderMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ChromeRenderMessageFilter>;

  ~ChromeRenderMessageFilter() override;

  void OnDnsPrefetch(const network_hints::LookupRequest& request);
  void OnPreconnect(int render_frame_id,
                    const GURL& url,
                    bool allow_credentials,
                    int count);

  void OnAllowDatabase(int render_frame_id,
                       const url::Origin& origin,
                       const GURL& site_for_cookies,
                       const url::Origin& top_frame_origin,
                       bool* allowed);
  void OnAllowDOMStorage(int render_frame_id,
                         const url::Origin& origin,
                         const GURL& site_for_cookies,
                         const url::Origin& top_frame_origin,
                         bool local,
                         bool* allowed);
  void OnRequestFileSystemAccessSync(int render_frame_id,
                                     const url::Origin& origin,
                                     const GURL& site_for_cookies,
                                     const url::Origin& top_frame_origin,
                                     IPC::Message* message);
  void OnRequestFileSystemAccessAsync(int render_frame_id,
                                      int request_id,
                                      const url::Origin& origin,
                                      const GURL& site_for_cookies,
                                      const url::Origin& top_frame_origin);
  void OnRequestFileSystemAccessSyncResponse(IPC::Message* reply_msg,
                                             bool allowed);
  void OnRequestFileSystemAccessAsyncResponse(int render_frame_id,
                                              int request_id,
                                              bool allowed);
  void OnRequestFileSystemAccess(int render_frame_id,
                                 const url::Origin& origin,
                                 const GURL& site_for_cookies,
                                 const url::Origin& top_frame_origin,
                                 base::Callback<void(bool)> callback);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static void FileSystemAccessedOnUIThread(int render_process_id,
                                           int render_frame_id,
                                           const GURL& url,
                                           bool allowed,
                                           base::Callback<void(bool)> callback);
  static void FileSystemAccessedResponse(int render_process_id,
                                         int render_frame_id,
                                         const GURL& url,
                                         base::Callback<void(bool)> callback,
                                         bool allowed);
#endif
  void OnAllowIndexedDB(int render_frame_id,
                        const url::Origin& origin,
                        const GURL& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        bool* allowed);
  void OnAllowCacheStorage(int render_frame_id,
                           const url::Origin& origin,
                           const GURL& site_for_cookies,
                           const url::Origin& top_frame_origin,
                           bool* allowed);
#if BUILDFLAG(ENABLE_PLUGINS)
  void OnIsCrashReportingEnabled(bool* enabled);
#endif

  const int render_process_id_;

  // The PreconnectManager for the associated Profile. This must only be
  // accessed on the UI thread.
  base::WeakPtr<predictors::PreconnectManager> preconnect_manager_;
  // Allows to check on the IO thread whether the PreconnectManager was
  // initialized.
  bool preconnect_manager_initialized_;

  // Used to look up permissions at database creation time.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
