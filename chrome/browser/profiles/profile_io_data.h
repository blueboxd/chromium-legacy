// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

class HostContentSettingsMap;

namespace chromeos {
class CertificateProvider;
}

namespace content_settings {
class CookieSettings;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
}

namespace extensions {
class InfoMap;
}

namespace net {
class ClientCertStore;
}  // namespace net

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile.  Profile owns ProfileIOData, but will make
// sure to delete it on the IO thread (except possibly in unit tests where there
// is no IO thread).
class ProfileIOData {
 public:
  virtual ~ProfileIOData();

  static ProfileIOData* FromResourceContext(content::ResourceContext* rc);

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);

  // Called by Profile.
  content::ResourceContext* GetResourceContext() const;

  // Initializes the ProfileIOData object.
  void Init() const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  extensions::InfoMap* GetExtensionInfoMap() const;
  content_settings::CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  StringPrefMember* google_services_account_id() const {
    return &google_services_user_account_id_;
  }

  // Gets Sync state, for Dice account consistency.
  bool IsSyncEnabled() const;

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

  StringListPrefMember* safe_browsing_whitelist_domains() const {
    return &safe_browsing_whitelist_domains_;
  }

  IntegerPrefMember* network_prediction_options() const {
    return &network_prediction_options_;
  }

  BooleanPrefMember* signed_exchange_enabled() const {
    return &signed_exchange_enabled_;
  }

  signin::AccountConsistencyMethod account_consistency() const {
    return account_consistency_;
  }

#if !defined(OS_CHROMEOS)
  std::string GetSigninScopedDeviceId() const;
#endif

#if defined(OS_CHROMEOS)
  std::string username_hash() const {
    return username_hash_;
  }
#endif

  Profile::ProfileType profile_type() const {
    return profile_type_;
  }

  bool IsOffTheRecord() const;

  BooleanPrefMember* force_google_safesearch() const {
    return &force_google_safesearch_;
  }

  IntegerPrefMember* force_youtube_restrict() const {
    return &force_youtube_restrict_;
  }

  StringPrefMember* allowed_domains_for_apps() const {
    return &allowed_domains_for_apps_;
  }

  IntegerPrefMember* incognito_availibility() const {
    return &incognito_availibility_pref_;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  BooleanPrefMember* always_open_pdf_externally() const {
    return &always_open_pdf_externally_;
  }
#endif

#if defined(OS_CHROMEOS)
  BooleanPrefMember* account_consistency_mirror_required() const {
    return &account_consistency_mirror_required_pref_;
  }
#endif

  void set_client_cert_store_factory_for_testing(
      const base::Callback<std::unique_ptr<net::ClientCertStore>()>& factory) {
    client_cert_store_factory_ = factory;
  }

  data_reduction_proxy::DataReductionProxyIOData*
  data_reduction_proxy_io_data() const {
    return data_reduction_proxy_io_data_.get();
  }

  ProtocolHandlerRegistry::IOThreadDelegate*
  protocol_handler_registry_io_thread_delegate() const {
    return protocol_handler_registry_io_thread_delegate_.get();
  }

  // Get platform ClientCertStore. May return nullptr.
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore();

 protected:
#if defined(OS_CHROMEOS)
  // Defines possible ways in which a profile may use the Chrome OS system
  // token.
  enum class SystemKeySlotUseType {
    // This profile does not use the system key slot.
    kNone,
    // This profile only uses the system key slot for client certiticates.
    kUseForClientAuth,
    // This profile uses the system key slot for client certificates and for
    // certificate management.
    kUseForClientAuthAndCertManagement
  };
#endif

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    base::FilePath path;

    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::InfoMap> extension_info_map;
#endif
    signin::AccountConsistencyMethod account_consistency =
        signin::AccountConsistencyMethod::kDisabled;

#if defined(OS_CHROMEOS)
    std::string username_hash;
    SystemKeySlotUseType system_key_slot_use_type = SystemKeySlotUseType::kNone;
    std::unique_ptr<chromeos::CertificateProvider> certificate_provider;
#endif

    // The profile this struct was populated from. It's passed as a void* to
    // ensure it's not accidently used on the IO thread. Before using it on the
    // UI thread, call ProfileManager::IsValidProfile to ensure it's alive.
    void* profile = nullptr;
  };

  explicit ProfileIOData(Profile::ProfileType profile_type);

  void InitializeOnUIThread(Profile* profile);

  // Called when the Profile is destroyed. Triggers destruction of the
  // ProfileIOData.
  void ShutdownOnUIThread();

  void set_data_reduction_proxy_io_data(
      std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
          data_reduction_proxy_io_data) const;

  bool initialized() const {
    return initialized_;
  }

  // Destroys the ResourceContext first, to cancel any URLRequests that are
  // using it still, before we destroy the member variables that those
  // URLRequests may be accessing.
  void DestroyResourceContext();

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(ProfileIOData* io_data);
    ~ResourceContext() override;

   private:
    friend class ProfileIOData;

    ProfileIOData* const io_data_;
  };

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // The order *DOES* matter for the majority of these member variables, so
  // don't move them around unless you know what you're doing!
  // General rules:
  //   * ResourceContext references the URLRequestContexts, so
  //   URLRequestContexts must outlive ResourceContext, hence ResourceContext
  //   should be destroyed first.
  //   * URLRequestContexts reference a whole bunch of members, so
  //   URLRequestContext needs to be destroyed before them.
  //   * Therefore, ResourceContext should be listed last, and then the
  //   URLRequestContexts, and then the URLRequestContext members.
  //   * Note that URLRequestContext members have a directed dependency graph
  //   too, so they must themselves be ordered correctly.

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

  // Used for testing.
  mutable base::Callback<std::unique_ptr<net::ClientCertStore>()>
      client_cert_store_factory_;

  mutable StringPrefMember google_services_user_account_id_;
  mutable BooleanPrefMember sync_requested_;
  mutable BooleanPrefMember sync_first_setup_complete_;
  mutable signin::AccountConsistencyMethod account_consistency_;

#if !defined(OS_CHROMEOS)
  mutable StringPrefMember signin_scoped_device_id_;
#endif

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember force_google_safesearch_;
  mutable IntegerPrefMember force_youtube_restrict_;
  mutable BooleanPrefMember safe_browsing_enabled_;
  mutable StringListPrefMember safe_browsing_whitelist_domains_;
  mutable StringPrefMember allowed_domains_for_apps_;
  mutable IntegerPrefMember network_prediction_options_;
  mutable IntegerPrefMember incognito_availibility_pref_;
  mutable BooleanPrefMember signed_exchange_enabled_;
#if BUILDFLAG(ENABLE_PLUGINS)
  mutable BooleanPrefMember always_open_pdf_externally_;
#endif
#if defined(OS_CHROMEOS)
  mutable BooleanPrefMember account_consistency_mirror_required_pref_;
#endif

  // Pointed to by URLRequestContext.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  mutable scoped_refptr<extensions::InfoMap> extension_info_map_;
#endif

  mutable std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data_;

  mutable scoped_refptr<ProtocolHandlerRegistry::IOThreadDelegate>
      protocol_handler_registry_io_thread_delegate_;

#if defined(OS_CHROMEOS)
  mutable std::string username_hash_;
  mutable SystemKeySlotUseType system_key_slot_use_type_;
  mutable std::unique_ptr<chromeos::CertificateProvider> certificate_provider_;
#endif

  mutable std::unique_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  const Profile::ProfileType profile_type_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
