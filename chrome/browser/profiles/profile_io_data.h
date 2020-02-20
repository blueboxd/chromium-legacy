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
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"

class HostContentSettingsMap;

namespace content_settings {
class CookieSettings;
}

namespace extensions {
class InfoMap;
}

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile.  Profile owns ProfileIOData, but will make
// sure to delete it on the IO thread (except possibly in unit tests where there
// is no IO thread).
class ProfileIOData {
 public:
  ProfileIOData();
  ~ProfileIOData();

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

#if defined(OS_CHROMEOS)
  std::string username_hash() const {
    return username_hash_;
  }
#endif

  void InitializeOnUIThread(Profile* profile);

  // Called when the Profile is destroyed. Triggers destruction of the
  // ProfileIOData.
  void ShutdownOnUIThread();

 protected:
  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::InfoMap> extension_info_map;
#endif

#if defined(OS_CHROMEOS)
    std::string username_hash;
    bool user_is_affiliated = false;
#endif
  };

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(ProfileIOData* io_data);
    ~ResourceContext() override;

   private:
    friend class ProfileIOData;

    ProfileIOData* const io_data_;
  };

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

  // Pointed to by URLRequestContext.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  mutable scoped_refptr<extensions::InfoMap> extension_info_map_;
#endif

#if defined(OS_CHROMEOS)
  mutable std::string username_hash_;
#endif

  mutable std::unique_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
