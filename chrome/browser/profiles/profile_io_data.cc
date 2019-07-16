// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/net_log/chrome_net_log.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_network_transaction_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/fileapi/external_file_protocol_handler.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/net/client_cert_filter_chromeos.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "services/network/cert_verifier_with_trust_anchors.h"
#include "services/network/cert_verify_proc_chromeos.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/ssl/client_cert_store_nss.h"
#endif  // defined(USE_NSS_CERTS)

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif  // defined(OS_MACOSX)

using content::BrowserContext;
using content::BrowserThread;
using content::ResourceContext;

namespace {

#if defined(OS_CHROMEOS)
// The following four functions are responsible for initializing NSS for each
// profile on ChromeOS, which has a separate NSS database and TPM slot
// per-profile.
//
// Initialization basically follows these steps:
// 1) Get some info from user_manager::UserManager about the User for this
// profile.
// 2) Tell nss_util to initialize the software slot for this profile.
// 3) Wait for the TPM module to be loaded by nss_util if it isn't already.
// 4) Ask CryptohomeClient which TPM slot id corresponds to this profile.
// 5) Tell nss_util to use that slot id on the TPM module.
//
// Some of these steps must happen on the UI thread, others must happen on the
// IO thread:
//               UI thread                              IO Thread
//
//  ProfileIOData::InitializeOnUIThread
//                   |
//  ProfileHelper::Get()->GetUserByProfile()
//                   \---------------------------------------v
//                                                 StartNSSInitOnIOThread
//                                                           |
//                                          crypto::InitializeNSSForChromeOSUser
//                                                           |
//                                                crypto::IsTPMTokenReady
//                                                           |
//                                          StartTPMSlotInitializationOnIOThread
//                   v---------------------------------------/
//     GetTPMInfoForUserOnUIThread
//                   |
// chromeos::TPMTokenInfoGetter::Start
//                   |
//     DidGetTPMInfoForUserOnUIThread
//                   \---------------------------------------v
//                                          crypto::InitializeTPMForChromeOSUser

void DidGetTPMInfoForUserOnUIThread(
    std::unique_ptr<chromeos::TPMTokenInfoGetter> getter,
    const std::string& username_hash,
    base::Optional<chromeos::CryptohomeClient::TpmTokenInfo> token_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (token_info.has_value() && token_info->slot != -1) {
    DVLOG(1) << "Got TPM slot for " << username_hash << ": "
             << token_info->slot;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&crypto::InitializeTPMForChromeOSUser, username_hash,
                       token_info->slot));
  } else {
    NOTREACHED() << "TPMTokenInfoGetter reported invalid token.";
  }
}

void GetTPMInfoForUserOnUIThread(const AccountId& account_id,
                                 const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Getting TPM info from cryptohome for "
           << " " << account_id.Serialize() << " " << username_hash;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> scoped_token_info_getter =
      chromeos::TPMTokenInfoGetter::CreateForUserToken(
          account_id, chromeos::CryptohomeClient::Get(),
          base::ThreadTaskRunnerHandle::Get());
  chromeos::TPMTokenInfoGetter* token_info_getter =
      scoped_token_info_getter.get();

  // Bind |token_info_getter| to the callback to ensure it does not go away
  // before TPM token info is fetched.
  // TODO(tbarzic, pneubeck): Handle this in a nicer way when this logic is
  //     moved to a separate profile service.
  token_info_getter->Start(base::BindOnce(&DidGetTPMInfoForUserOnUIThread,
                                          std::move(scoped_token_info_getter),
                                          username_hash));
}

void StartTPMSlotInitializationOnIOThread(const AccountId& account_id,
                                          const std::string& username_hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetTPMInfoForUserOnUIThread, account_id, username_hash));
}

void StartNSSInitOnIOThread(const AccountId& account_id,
                            const std::string& username_hash,
                            const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Starting NSS init for " << account_id.Serialize()
           << "  hash:" << username_hash;

  // Make sure NSS is initialized for the user.
  crypto::InitializeNSSForChromeOSUser(username_hash, path);

  // Check if it's OK to initialize TPM for the user before continuing. This
  // may not be the case if the TPM slot initialization was previously
  // requested for the same user.
  if (!crypto::ShouldInitializeTPMForChromeOSUser(username_hash))
    return;

  crypto::WillInitializeTPMForChromeOSUser(username_hash);

  if (crypto::IsTPMTokenEnabledForNSS()) {
    if (crypto::IsTPMTokenReady(
            base::Bind(&StartTPMSlotInitializationOnIOThread, account_id,
                       username_hash))) {
      StartTPMSlotInitializationOnIOThread(account_id, username_hash);
    } else {
      DVLOG(1) << "Waiting for tpm ready ...";
    }
  } else {
    crypto::InitializePrivateSoftwareSlotForChromeOSUser(username_hash);
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

void ProfileIOData::InitializeOnUIThread(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* pref_service = profile->GetPrefs();

  std::unique_ptr<ProfileParams> params(new ProfileParams);
  params->path = profile->GetPath();

  params->cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  params->host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  params->extension_info_map =
      extensions::ExtensionSystem::Get(profile)->info_map();
#endif

  params->account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);

  ProtocolHandlerRegistry* protocol_handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
  DCHECK(protocol_handler_registry);

  protocol_handler_registry_io_thread_delegate_ =
      protocol_handler_registry->io_thread_delegate();

#if defined(OS_CHROMEOS)
  // Enable client certificates for the Chrome OS sign-in frame, if this feature
  // is not disabled by a flag.
  // Note that while this applies to the whole sign-in profile, client
  // certificates will only be selected for the StoragePartition currently used
  // in the sign-in frame (see SigninPartitionManager).
  if (chromeos::switches::IsSigninFrameClientCertsEnabled() &&
      chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // We only need the system slot for client certificates, not in NSS context
    // (the sign-in profile's NSS context is not initialized).
    params->system_key_slot_use_type = SystemKeySlotUseType::kUseForClientAuth;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      params->username_hash = user->username_hash();
      DCHECK(!params->username_hash.empty());
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&StartNSSInitOnIOThread, user->GetAccountId(),
                         user->username_hash(), profile->GetPath()));

      // Use the device-wide system key slot only if the user is affiliated on
      // the device.
      if (user->IsAffiliated()) {
        params->system_key_slot_use_type =
            SystemKeySlotUseType::kUseForClientAuthAndCertManagement;
      }
    }
  }

  chromeos::CertificateProviderService* cert_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile);
  if (cert_provider_service) {
    params->certificate_provider =
        cert_provider_service->CreateCertificateProvider();
  }
#endif

  params->profile = profile;
  profile_params_ = std::move(params);

  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_google_safesearch_.MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  force_youtube_restrict_.Init(prefs::kForceYouTubeRestrict, pref_service);
  force_youtube_restrict_.MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  allowed_domains_for_apps_.Init(prefs::kAllowedDomainsForApps, pref_service);
  allowed_domains_for_apps_.MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  signed_exchange_enabled_.Init(prefs::kSignedHTTPExchangeEnabled,
                                pref_service);
  signed_exchange_enabled_.MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  // These members are used only for sign in, which is not enabled in incognito
  // and guest modes. So no need to initialize them.
  if (!IsOffTheRecord()) {
    google_services_user_account_id_.Init(prefs::kGoogleServicesUserAccountId,
                                          pref_service);
    google_services_user_account_id_.MoveToSequence(io_task_runner);
    sync_requested_.Init(syncer::prefs::kSyncRequested, pref_service);
    sync_requested_.MoveToSequence(io_task_runner);
    sync_first_setup_complete_.Init(syncer::prefs::kSyncFirstSetupComplete,
                                    pref_service);
    sync_first_setup_complete_.MoveToSequence(io_task_runner);
  }

#if !defined(OS_CHROMEOS)
  signin_scoped_device_id_.Init(prefs::kGoogleServicesSigninScopedDeviceId,
                                pref_service);
  signin_scoped_device_id_.MoveToSequence(io_task_runner);
#endif

  network_prediction_options_.Init(prefs::kNetworkPredictionOptions,
                                   pref_service);

  network_prediction_options_.MoveToSequence(io_task_runner);

  incognito_availibility_pref_.Init(prefs::kIncognitoModeAvailability,
                                    pref_service);
  incognito_availibility_pref_.MoveToSequence(io_task_runner);

#if defined(OS_CHROMEOS)
  account_consistency_mirror_required_pref_.Init(
      prefs::kAccountConsistencyMirrorRequired, pref_service);
  account_consistency_mirror_required_pref_.MoveToSequence(io_task_runner);
#endif

  // We need to make sure that content initializes its own data structures that
  // are associated with each ResourceContext because we might post this
  // object to the IO thread after this function.
  BrowserContext::EnsureResourceContextInitialized(profile);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ProfileIOData::Init, base::Unretained(this)));
}

ProfileIOData::ProfileParams::ProfileParams() = default;

ProfileIOData::ProfileParams::~ProfileParams() = default;

ProfileIOData::ProfileIOData(Profile::ProfileType profile_type)
    : initialized_(false),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
#if defined(OS_CHROMEOS)
      system_key_slot_use_type_(SystemKeySlotUseType::kNone),
#endif
      resource_context_(new ResourceContext(this)),
      profile_type_(profile_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ProfileIOData::~ProfileIOData() {
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO))
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
ProfileIOData* ProfileIOData::FromResourceContext(
    content::ResourceContext* rc) {
  return (static_cast<ResourceContext*>(rc))->io_data_;
}

// static
bool ProfileIOData::IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kFileScheme,
    content::kChromeDevToolsScheme,
    dom_distiller::kDomDistillerScheme,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::kExtensionScheme,
#endif
    content::kChromeUIScheme,
    url::kDataScheme,
#if defined(OS_CHROMEOS)
    content::kExternalFileScheme,
#endif  // defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
    url::kContentScheme,
#endif  // defined(OS_ANDROID)
    url::kAboutScheme,
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kFtpScheme,
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kBlobScheme,
    url::kFileSystemScheme,
    chrome::kChromeSearchScheme,
  };
  for (size_t i = 0; i < base::size(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
}

// static
bool ProfileIOData::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

content::ResourceContext* ProfileIOData::GetResourceContext() const {
  return resource_context_.get();
}

extensions::InfoMap* ProfileIOData::GetExtensionInfoMap() const {
  DCHECK(initialized_) << "ExtensionSystem not initialized";
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extension_info_map_.get();
#else
  return nullptr;
#endif
}

content_settings::CookieSettings* ProfileIOData::GetCookieSettings() const {
  // Allow either Init() or SetCookieSettingsForTesting() to initialize.
  DCHECK(initialized_ || cookie_settings_.get());
  return cookie_settings_.get();
}

HostContentSettingsMap* ProfileIOData::GetHostContentSettingsMap() const {
  DCHECK(initialized_);
  return host_content_settings_map_.get();
}

bool ProfileIOData::IsSyncEnabled() const {
  return sync_first_setup_complete_.GetValue() && sync_requested_.GetValue();
}

#if !defined(OS_CHROMEOS)
std::string ProfileIOData::GetSigninScopedDeviceId() const {
  return signin_scoped_device_id_.GetValue();
}
#endif

bool ProfileIOData::IsOffTheRecord() const {
  return profile_type() == Profile::INCOGNITO_PROFILE ||
         profile_type() == Profile::GUEST_PROFILE;
}

std::unique_ptr<net::ClientCertStore> ProfileIOData::CreateClientCertStore() {
  if (!client_cert_store_factory_.is_null())
    return client_cert_store_factory_.Run();
#if defined(OS_CHROMEOS)
  bool use_system_key_slot =
      system_key_slot_use_type_ == SystemKeySlotUseType::kUseForClientAuth ||
      system_key_slot_use_type_ ==
          SystemKeySlotUseType::kUseForClientAuthAndCertManagement;
  return std::unique_ptr<net::ClientCertStore>(
      new chromeos::ClientCertStoreChromeOS(
          certificate_provider_ ? certificate_provider_->Copy() : nullptr,
          std::make_unique<chromeos::ClientCertFilterChromeOS>(
              use_system_key_slot, username_hash_),
          base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                     kCryptoModulePasswordClientAuth)));
#elif defined(USE_NSS_CERTS)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                 kCryptoModulePasswordClientAuth)));
#elif defined(OS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MACOSX)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#elif defined(OS_ANDROID)
  // Android does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return nullptr;
#else
#error Unknown platform.
#endif
}

void ProfileIOData::set_data_reduction_proxy_io_data(
    std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
        data_reduction_proxy_io_data) const {
  data_reduction_proxy_io_data_ = std::move(data_reduction_proxy_io_data);
}

ProfileIOData::ResourceContext::ResourceContext(ProfileIOData* io_data)
    : io_data_(io_data) {
  DCHECK(io_data);
}

ProfileIOData::ResourceContext::~ResourceContext() {}

void ProfileIOData::Init() const {
  // The basic logic is implemented here. The specific initialization
  // is done in InitializeInternal(), implemented by subtypes. Static helper
  // functions have been provided to assist in common operations.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);
  DCHECK(profile_params_.get());

  account_consistency_ = profile_params_->account_consistency;

  // Take ownership over these parameters.
  cookie_settings_ = profile_params_->cookie_settings;
  host_content_settings_map_ = profile_params_->host_content_settings_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_info_map_ = profile_params_->extension_info_map;
#endif

#if defined(OS_CHROMEOS)
  username_hash_ = profile_params_->username_hash;
  system_key_slot_use_type_ = profile_params_->system_key_slot_use_type;
  // If we're using the system slot for certificate management, we also must
  // have access to the user's slots.
  DCHECK(!(username_hash_.empty() &&
           system_key_slot_use_type_ ==
               SystemKeySlotUseType::kUseForClientAuthAndCertManagement));
  if (system_key_slot_use_type_ ==
      SystemKeySlotUseType::kUseForClientAuthAndCertManagement) {
    EnableNSSSystemKeySlotForResourceContext(resource_context_.get());
  }

  certificate_provider_ = std::move(profile_params_->certificate_provider);
#endif

  profile_params_.reset();
  initialized_ = true;
}

void ProfileIOData::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  google_services_user_account_id_.Destroy();
  sync_requested_.Destroy();
  sync_first_setup_complete_.Destroy();
#if !defined(OS_CHROMEOS)
  signin_scoped_device_id_.Destroy();
#endif
  force_google_safesearch_.Destroy();
  force_youtube_restrict_.Destroy();
  allowed_domains_for_apps_.Destroy();
  safe_browsing_enabled_.Destroy();
  safe_browsing_whitelist_domains_.Destroy();
  network_prediction_options_.Destroy();
  incognito_availibility_pref_.Destroy();
  signed_exchange_enabled_.Destroy();
#if BUILDFLAG(ENABLE_PLUGINS)
  always_open_pdf_externally_.Destroy();
#endif
#if defined(OS_CHROMEOS)
  account_consistency_mirror_required_pref_.Destroy();
#endif

  bool posted = BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  if (!posted)
    delete this;
}

void ProfileIOData::DestroyResourceContext() {
  resource_context_.reset();
}
