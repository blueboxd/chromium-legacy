// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class CookieStore;
class CanonicalCookie;
}

namespace android_webview {

// CookieManager creates and owns Webview's CookieStore, in addition to handling
// calls into the CookieStore from Java.
//
// Since Java calls can be made on the IO Thread, and must synchronously return
// a result, and the CookieStore API allows it to asynchronously return results,
// the CookieStore must be run on its own thread, to prevent deadlock.
class CookieManager {
 public:
  static CookieManager* GetInstance();

  // Passes a |cookie_manager_remote|, which this will use for CookieManager
  // APIs going forward. Only called in the Network Service path, with the
  // intention this is called once during content initialization (when we create
  // the only NetworkContext). Note: no other cookie tasks will be processed
  // while this operation is running.
  void SetMojoCookieManager(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote);

  void SetShouldAcceptCookies(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jboolean accept);
  jboolean GetShouldAcceptCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jstring>& value,
                 const base::android::JavaParamRef<jobject>& java_callback);
  void SetCookieSync(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& url,
                     const base::android::JavaParamRef<jstring>& value);

  base::android::ScopedJavaLocalRef<jstring> GetCookie(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url);

  void RemoveAllCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_callback);
  void RemoveSessionCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_callback);
  void RemoveAllCookiesSync(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void RemoveSessionCookiesSync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RemoveExpiredCookies(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void FlushCookieStore(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jboolean HasCookies(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  bool AllowFileSchemeCookies();
  jboolean AllowFileSchemeCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetAcceptFileSchemeCookies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean accept);

  base::FilePath GetCookieStorePath();

 private:
  friend struct base::LazyInstanceTraitsBase<CookieManager>;

  CookieManager();
  ~CookieManager();

  // Returns the CookieStore, creating it if necessary. This must only be called
  // on the CookieStore TaskRunner.
  net::CookieStore* GetCookieStore();

  // Gets the Network Service CookieManager if it's been passed via
  // |SetMojoCookieManager|. Otherwise (if Network Service is disabled or
  // content layer has not yet initialized the NetworkContext), this returns
  // nullptr (and |GetCookieStore| should be used installed). This must only be
  // called on the CookieStore TaskRunner.
  network::mojom::CookieManager* GetMojoCookieManager();

  void ExecCookieTaskSync(
      base::OnceCallback<void(base::OnceCallback<void(bool)>)> task);
  void ExecCookieTaskSync(
      base::OnceCallback<void(base::OnceCallback<void(int)>)> task);
  void ExecCookieTaskSync(base::OnceCallback<void(base::OnceClosure)> task);
  void ExecCookieTask(base::OnceClosure task);
  // Runs all queued-up cookie tasks in |tasks_|.
  void RunPendingCookieTasks();

  void SetCookieHelper(const GURL& host,
                       const std::string& value,
                       base::OnceCallback<void(bool)> callback);

  void GotCookies(const std::vector<net::CanonicalCookie>& cookies);
  void GetCookieListAsyncHelper(const GURL& host,
                                net::CookieList* result,
                                base::OnceClosure complete);
  void GetCookieListCompleted(base::OnceClosure complete,
                              net::CookieList* result,
                              const net::CookieStatusList& value,
                              const net::CookieStatusList& excluded_cookies);

  void RemoveSessionCookiesHelper(base::OnceCallback<void(bool)> callback);
  void RemoveAllCookiesHelper(base::OnceCallback<void(bool)> callback);
  void RemoveCookiesCompleted(base::OnceCallback<void(bool)> callback,
                              uint32_t num_deleted);

  void FlushCookieStoreAsyncHelper(base::OnceClosure complete);

  void SetMojoCookieManagerAsync(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
      base::OnceClosure complete);
  void SwapMojoCookieManagerAsync(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote,
      base::OnceClosure complete);

  void HasCookiesAsyncHelper(bool* result, base::OnceClosure complete);
  void HasCookiesCompleted(base::OnceClosure complete,
                           bool* result,
                           const net::CookieList& cookies);

  // |result| indicates whether or not this call was successful, indicating
  // whether we may update |accept_file_scheme_cookies_|.
  void AllowFileSchemeCookiesAsyncHelper(bool accept,
                                         bool* result,
                                         base::OnceClosure complete);
  void AllowFileSchemeCookiesCompleted(base::OnceClosure complete,
                                       bool* result,
                                       bool value);
  void MigrateCookieStorePath();

  base::FilePath cookie_store_path_;

  // This protects the following two bools, as they're used on multiple threads.
  base::Lock accept_file_scheme_cookies_lock_;
  // True if cookies should be allowed for file URLs. Can only be changed prior
  // to creating the CookieStore.
  bool accept_file_scheme_cookies_;
  // True once the cookie store has been created. Just used to track when
  // |accept_file_scheme_cookies_| can no longer be modified.
  bool cookie_store_created_;

  base::Thread cookie_store_client_thread_;
  base::Thread cookie_store_backend_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> cookie_store_task_runner_;
  std::unique_ptr<net::CookieStore> cookie_store_;

  // Tracks if we're in the middle of a call to SetMojoCookieManager(). See the
  // note in SetMojoCookieManager(). Must only be accessed on
  // |cookie_store_task_runner_|.
  bool setting_new_mojo_cookie_manager_;

  // |tasks_| is a queue we manage, to allow us to delay tasks until after
  // SetMojoCookieManager()'s work is done. This is modified on different
  // threads, so accesses must be guarded by |task_queue_lock_|.
  base::Lock task_queue_lock_;
  base::circular_deque<base::OnceClosure> tasks_;

  // The CookieManager shared with the NetworkContext.
  mojo::Remote<network::mojom::CookieManager> mojo_cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(CookieManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COOKIE_MANAGER_H_
