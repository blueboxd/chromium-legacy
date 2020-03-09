// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "ios/web/public/thread/web_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class PrefChangeRegistrar;
class PrefService;

namespace base {
class FilePath;
}

namespace net {
class URLRequestContext;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

// Manages Safe Browsing related functionality. This class owns and provides
// support for constructing and initializing the Safe Browsing database. This
// class must be created and destroyed on the main thread. This class is
// RefCounted so that PostTask'd calls into this class can retain a reference
// to an instance.
class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService,
                                        web::WebThread::DeleteOnUIThread> {
 public:
  SafeBrowsingService();

  // Called on the UI thread to initialize the service.
  void Initialize(PrefService* prefs, const base::FilePath& user_data_path);

  // Called on the UI thread to terminate the service. This must be called
  // before the IO thread is torn down.
  void ShutDown();

  // Returns the SafeBrowsingDatabaseManager owned by this service.
  safe_browsing::SafeBrowsingDatabaseManager* GetDatabaseManager();

 private:
  friend struct web::WebThread::DeleteOnThread<web::WebThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingService>;

  // A helper class for enabling/disabling Safe Browsing and maintaining state
  // on the IO thread. This class may be constructed and destroyed on the UI
  // thread, but all of its other methods should only be called on the IO
  // thread.
  class IOThreadEnabler : public base::RefCountedThreadSafe<IOThreadEnabler> {
   public:
    IOThreadEnabler(scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
                        database_manager);

    // Creates the network context and URL loader factory used by the
    // SafeBrowsingDatabaseManager.
    void Initialize(scoped_refptr<SafeBrowsingService> safe_browsing_service,
                    mojo::PendingReceiver<network::mojom::NetworkContext>
                        network_context_receiver);

    // Disables Safe Browsing, and destroys the network context and URL loader
    // factory used by the SafeBrowsingDatabaseManager.
    void ShutDown();

    // Enables or disables Safe Browsing database updates and lookups.
    void SetSafeBrowsingEnabled(bool enabled);

   private:
    friend base::RefCountedThreadSafe<IOThreadEnabler>;
    ~IOThreadEnabler();

    // Starts the SafeBrowsingDatabaseManager, making it ready to accept
    // queries.
    void StartSafeBrowsingDBManager();

    // Constructs a URLRequestContext.
    void SetUpURLRequestContext();

    // Constructs a SharedURLLoaderFactory.
    void SetUpURLLoaderFactory(
        scoped_refptr<SafeBrowsingService> safe_browsing_service);

    // This tracks whether the service is running.
    bool enabled_ = false;

    // This tracks whether ShutDown() has been called.
    bool shutting_down_ = false;

    // This is wrapped by |network_context|.
    std::unique_ptr<net::URLRequestContext> url_request_context_;

    // The network context used for Safe Browsing related network requests.
    std::unique_ptr<network::NetworkContext> network_context_;

    // An IO thread remote for a URLLoaderFactory created on the UI thread.
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

    // A SharedURLLoaderFactory that wraps |url_loader_factory_|.
    scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
        shared_url_loader_factory_;

    // The database manager used for Safe Browsing queries.
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
        safe_browsing_db_manager_;

    DISALLOW_COPY_AND_ASSIGN(IOThreadEnabler);
  };

  ~SafeBrowsingService();

  // Called on the UI thread to construct a URLLoaderFactory that is used on
  // the IO thread.
  void SetUpURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // Enables or disables Safe Browsing, depending on the current state of
  // preferences.
  void UpdateSafeBrowsingEnabledState();
  // This is the UI thread remote for IOThreadState's network context.
  mojo::Remote<network::mojom::NetworkContext> network_context_client_;

  // Constructed on the UI thread, but otherwise its methods are only called on
  // the IO thread.
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;

  // This watches for changes to the Safe Browsing opt-out preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Encapsulates methods and objects that are used on the IO thread.
  scoped_refptr<IOThreadEnabler> io_thread_enabler_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
