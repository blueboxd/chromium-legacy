// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_cookies.h"

#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/cookie_config/cookie_store_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "extensions/common/constants.h"
#include "services/network/cookie_manager.h"
#include "services/network/restricted_cookie_manager.h"

namespace extensions {

ChromeExtensionCookies::ChromeExtensionCookies(Profile* profile)
    : profile_(profile), cookie_settings_observer_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  cookie_settings_observer_.Add(cookie_settings_.get());
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  std::unique_ptr<content::CookieStoreConfig> creation_config;
  if (profile_->IsIncognitoProfile() || profile_->AsTestingProfile()) {
    creation_config = std::make_unique<content::CookieStoreConfig>();
  } else {
    creation_config = std::make_unique<content::CookieStoreConfig>(
        profile_->GetPath().Append(chrome::kExtensionsCookieFilename),
        profile_->ShouldRestoreOldSessionCookies(),
        profile_->ShouldPersistSessionCookies(), nullptr /* storage_policy */);
    creation_config->crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  }
  creation_config->cookieable_schemes.push_back(extensions::kExtensionScheme);

  network::mojom::CookieManagerParamsPtr initial_settings =
      ProfileNetworkContextService::CreateCookieManagerParams(
          profile_, *cookie_settings_);

  io_data_ = std::make_unique<IOData>(std::move(creation_config),
                                      std::move(initial_settings));
}

ChromeExtensionCookies::~ChromeExtensionCookies() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!io_data_);
}

// static
ChromeExtensionCookies* ChromeExtensionCookies::Get(
    content::BrowserContext* context) {
  return ChromeExtensionCookiesFactory::GetForBrowserContext(context);
}

void ChromeExtensionCookies::CreateRestrictedCookieManager(
    const url::Origin& origin,
    network::mojom::RestrictedCookieManagerRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)
    return;

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&IOData::CreateRestrictedCookieManager,
                     base::Unretained(io_data_.get()), origin,
                     std::move(request)));
}

void ChromeExtensionCookies::ClearCookies(const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&IOData::ClearCookies, base::Unretained(io_data_.get()),
                     origin));
}

net::CookieStore* ChromeExtensionCookies::GetCookieStoreForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!io_data_)  // null after shutdown.
    return nullptr;

  return io_data_->GetOrCreateCookieStore();
}

ChromeExtensionCookies::IOData::IOData(
    std::unique_ptr<content::CookieStoreConfig> creation_config,
    network::mojom::CookieManagerParamsPtr initial_mojo_cookie_settings)
    : creation_config_(std::move(creation_config)),
      mojo_cookie_settings_(std::move(initial_mojo_cookie_settings)) {
  UpdateNetworkCookieSettings();
}

ChromeExtensionCookies::IOData::~IOData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeExtensionCookies::IOData::CreateRestrictedCookieManager(
    const url::Origin& origin,
    network::mojom::RestrictedCookieManagerRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  restricted_cookie_managers_.AddBinding(
      std::make_unique<network::RestrictedCookieManager>(
          network::mojom::RestrictedCookieManagerRole::SCRIPT,
          GetOrCreateCookieStore(), &network_cookie_settings_, origin,
          /* null network_context_client disables logging, making later
             arguments irrelevant */
          nullptr, false, -1, -1),
      std::move(request));
}

void ChromeExtensionCookies::IOData::ClearCookies(const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieDeletionInfo delete_info;
  delete_info.host = origin.host();
  GetOrCreateCookieStore()->DeleteAllMatchingInfoAsync(
      std::move(delete_info), net::CookieStore::DeleteCallback());
}

void ChromeExtensionCookies::IOData::OnContentSettingChanged(
    ContentSettingsForOneType settings) {
  mojo_cookie_settings_->settings = std::move(settings);
  UpdateNetworkCookieSettings();
}

void ChromeExtensionCookies::IOData::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  mojo_cookie_settings_->block_third_party_cookies = block_third_party_cookies;
  UpdateNetworkCookieSettings();
}

net::CookieStore* ChromeExtensionCookies::IOData::GetOrCreateCookieStore() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!cookie_store_) {
    cookie_store_ =
        content::CreateCookieStore(*creation_config_, nullptr /* netlog */);
  }
  return cookie_store_.get();
}

void ChromeExtensionCookies::IOData::UpdateNetworkCookieSettings() {
  network::CookieManager::ConfigureCookieSettings(*mojo_cookie_settings_,
                                                  &network_cookie_settings_);
}

void ChromeExtensionCookies::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES &&
      content_type != CONTENT_SETTINGS_TYPE_DEFAULT) {
    return;
  }

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &settings);

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&IOData::OnContentSettingChanged,
                     base::Unretained(io_data_.get()), std::move(settings)));
}

void ChromeExtensionCookies::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&IOData::OnThirdPartyCookieBlockingChanged,
                     base::Unretained(io_data_.get()),
                     block_third_party_cookies));
}

void ChromeExtensionCookies::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Async delete on IO thread, sequencing it after any previously posted
  // operations.
  //
  // Note: during tests this may be called with IO thread == UI thread. If this
  // were to use unique_ptr<.., DeleteOnIOThread> that case would result in
  // unwanted synchronous deletion; hence DeleteSoon is used by hand.
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     std::move(io_data_));
  profile_ = nullptr;
}

}  // namespace extensions
