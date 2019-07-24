// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/base/features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace {

bool* g_discard_domain_reliability_uploads_for_testing = nullptr;

std::vector<std::string> TranslateStringArray(const base::ListValue* list) {
  std::vector<std::string> strings;
  for (const base::Value& value : *list) {
    DCHECK(value.is_string());
    strings.push_back(value.GetString());
  }
  return strings;
}

std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
          ? net::HttpUtil::ExpandLanguageList(language_pref)
          : language_pref;
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

#if defined(OS_CHROMEOS)
Profile* GetPrimaryProfile() {
  // Obtains the primary profile.
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  return chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

bool ShouldUseBuiltinCertVerifier(Profile* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kForceCertVerifierBuiltin))
    return true;

  if (chromeos::ProfileHelper::Get()->IsSigninProfile(profile) ||
      chromeos::ProfileHelper::Get()->IsLockScreenAppProfile(profile)) {
    // No need to override the feature-set setting through policy for sign-in
    // and lock screen app profiles, as no custom trust anchors can be active
    // there.
    return base::FeatureList::IsEnabled(
        net::features::kCertVerifierBuiltinFeature);
  }

  // TODO(https://crbug.com/982936): Instead of evaluating the primary profile
  // prefs explicitly, register the pref with LocalState and evaluate the pref
  // there. Note that currently that is not possible because the LocalState
  // prefs may not reflect the primary profile policy state at the time this
  // function is executed.

  // Explicitly check if |profile| is the primary profile. GetPrimaryProfile
  // only returns non-null pretty late in the primary profile initialization
  // stage, and the NetworkContext is created before that.
  Profile* primary_profile = nullptr;
  if (chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile))
    primary_profile = profile;

  if (!primary_profile)
    primary_profile = GetPrimaryProfile();
  if (!primary_profile) {
    NOTREACHED();
    return base::FeatureList::IsEnabled(
        net::features::kCertVerifierBuiltinFeature);
  }

  // The policy evaluated through the pref is not updatable dynamically, so
  // assert that the pref system is already initialized at the time this is
  // called.
  DCHECK_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            primary_profile->GetPrefs()->GetInitializationStatus());
  const PrefService::Preference* builtin_cert_verifier_enabled_pref =
      primary_profile->GetPrefs()->FindPreference(
          prefs::kBuiltinCertificateVerifierEnabled);
  if (builtin_cert_verifier_enabled_pref->IsManaged())
    return builtin_cert_verifier_enabled_pref->GetValue()->GetBool();

  // TODO(https://crbug.com/939344): Also evaluate whether there are
  // extension-specific certificates to be used, and if yes, enable the built-in
  // cert verifier.
  return base::FeatureList::IsEnabled(
      net::features::kCertVerifierBuiltinFeature);
}
#endif  // defined (OS_CHROMEOS)

}  // namespace

ProfileNetworkContextService::ProfileNetworkContextService(Profile* profile)
    : profile_(profile),
      proxy_config_monitor_(profile),
      cookie_settings_observer_(this) {
  PrefService* profile_prefs = profile->GetPrefs();
  quic_allowed_.Init(
      prefs::kQuicAllowed, profile_prefs,
      base::Bind(&ProfileNetworkContextService::DisableQuicIfNotAllowed,
                 base::Unretained(this)));
  pref_accept_language_.Init(
      language::prefs::kAcceptLanguages, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateAcceptLanguage,
                          base::Unretained(this)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateReferrersEnabled,
                          base::Unretained(this)));
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  cookie_settings_observer_.Add(cookie_settings_.get());

  DisableQuicIfNotAllowed();

  // Observe content settings so they can be synced to the network service.
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  pref_change_registrar_.Init(profile_prefs);

#if defined(OS_CHROMEOS)
  using_builtin_cert_verifier_ = ShouldUseBuiltinCertVerifier(profile_);
  VLOG(0) << "Using " << (using_builtin_cert_verifier_ ? "built-in" : "legacy")
          << " cert verifier.";
#endif
  // When any of the following CT preferences change, we schedule an update
  // to aggregate the actual update using a |ct_policy_update_timer_|.
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTRequiredHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedSPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedLegacySPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
}

ProfileNetworkContextService::~ProfileNetworkContextService() {}

network::mojom::NetworkContextPtr
ProfileNetworkContextService::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  network::mojom::NetworkContextPtr network_context;

  content::GetNetworkService()->CreateNetworkContext(
      MakeRequest(&network_context),
      CreateNetworkContextParams(in_memory, relative_partition_path));

  if ((!in_memory && !profile_->IsOffTheRecord())) {
    // TODO(jam): delete this code 1 year after Network Service shipped to all
    // stable users, which would be after M83 branches.
    base::FilePath media_cache_path = GetPartitionPath(relative_partition_path)
                                          .Append(chrome::kMediaCacheDirname);
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), media_cache_path,
                       true /* recursive */));
  }

  std::vector<network::mojom::NetworkContext*> contexts{network_context.get()};
  UpdateCTPolicyForContexts(contexts);

  return network_context;
}

#if defined(OS_CHROMEOS)
void ProfileNetworkContextService::UpdateAdditionalCertificates(
    const net::CertificateList& all_additional_certificates,
    const net::CertificateList& trust_anchors) {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](const net::CertificateList& all_additional_certificates,
                       const net::CertificateList& trust_anchors,
                       content::StoragePartition* storage_partition) {
                      auto additional_certificates =
                          network::mojom::AdditionalCertificates::New();
                      additional_certificates->all_certificates =
                          all_additional_certificates;
                      additional_certificates->trust_anchors = trust_anchors;
                      storage_partition->GetNetworkContext()
                          ->UpdateAdditionalCertificates(
                              std::move(additional_certificates));
                    },
                    all_additional_certificates, trust_anchors));
}
#endif

void ProfileNetworkContextService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kQuicAllowed, true);
#if defined(OS_CHROMEOS)
  // Note that the default value is not relevant because the pref is only
  // evaluated when it is managed.
  registry->RegisterBooleanPref(prefs::kBuiltinCertificateVerifierEnabled,
                                false);
#endif
}

void ProfileNetworkContextService::DisableQuicIfNotAllowed() {
  if (!quic_allowed_.IsManaged())
    return;

  // If QUIC is allowed, do nothing (re-enabling QUIC is not supported).
  if (quic_allowed_.GetValue())
    return;

  g_browser_process->system_network_context_manager()->DisableQuic();
}

void ProfileNetworkContextService::UpdateAcceptLanguage() {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](const std::string& accept_language,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetNetworkContext()->SetAcceptLanguage(
                          accept_language);
                    },
                    ComputeAcceptLanguage()));
}

void ProfileNetworkContextService::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](bool block_third_party_cookies,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetCookieManagerForBrowserProcess()
                          ->BlockThirdPartyCookies(block_third_party_cookies);
                    },
                    block_third_party_cookies));
}

std::string ProfileNetworkContextService::ComputeAcceptLanguage() const {
  return ComputeAcceptLanguageFromPref(pref_accept_language_.GetValue());
}

void ProfileNetworkContextService::UpdateReferrersEnabled() {
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](bool enable_referrers,
             content::StoragePartition* storage_partition) {
            storage_partition->GetNetworkContext()->SetEnableReferrers(
                enable_referrers);
          },
          enable_referrers_.GetValue()));
}

void ProfileNetworkContextService::UpdateCTPolicyForContexts(
    const std::vector<network::mojom::NetworkContext*>& contexts) {
  auto* prefs = profile_->GetPrefs();
  const base::ListValue* ct_required =
      prefs->GetList(certificate_transparency::prefs::kCTRequiredHosts);
  const base::ListValue* ct_excluded =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedHosts);
  const base::ListValue* ct_excluded_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedSPKIs);
  const base::ListValue* ct_excluded_legacy_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedLegacySPKIs);

  std::vector<std::string> required(TranslateStringArray(ct_required));
  std::vector<std::string> excluded(TranslateStringArray(ct_excluded));
  std::vector<std::string> excluded_spkis(
      TranslateStringArray(ct_excluded_spkis));
  std::vector<std::string> excluded_legacy_spkis(
      TranslateStringArray(ct_excluded_legacy_spkis));

  for (auto* context : contexts) {
    context->SetCTPolicy(required, excluded, excluded_spkis,
                         excluded_legacy_spkis);
  }
}

void ProfileNetworkContextService::UpdateCTPolicy() {
  std::vector<network::mojom::NetworkContext*> contexts;
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](std::vector<network::mojom::NetworkContext*>* contexts_ptr,
             content::StoragePartition* storage_partition) {
            contexts_ptr->push_back(storage_partition->GetNetworkContext());
          },
          &contexts));

  UpdateCTPolicyForContexts(contexts);
}

void ProfileNetworkContextService::ScheduleUpdateCTPolicy() {
  ct_policy_update_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(0),
                                this,
                                &ProfileNetworkContextService::UpdateCTPolicy);
}

// static
network::mojom::CookieManagerParamsPtr
ProfileNetworkContextService::CreateCookieManagerParams(
    Profile* profile,
    const content_settings::CookieSettings& cookie_settings) {
  auto out = network::mojom::CookieManagerParams::New();
  out->block_third_party_cookies =
      cookie_settings.ShouldBlockThirdPartyCookies();
  out->secure_origin_cookies_allowed_schemes.push_back(
      content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  out->third_party_cookies_allowed_schemes.push_back(
      extensions::kExtensionScheme);
#endif

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &settings);
  out->settings = std::move(settings);
  return out;
}

void ProfileNetworkContextService::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void ProfileNetworkContextService::SetDiscardDomainReliabilityUploadsForTesting(
    bool value) {
  g_discard_domain_reliability_uploads_for_testing = new bool(value);
}

network::mojom::NetworkContextParamsPtr
ProfileNetworkContextService::CreateNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  if (profile_->IsOffTheRecord())
    in_memory = true;
  base::FilePath path(GetPartitionPath(relative_partition_path));

  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("main");

  network_context_params->accept_language = ComputeAcceptLanguage();
  network_context_params->enable_referrers = enable_referrers_.GetValue();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kShortReportingDelay)) {
    network_context_params->reporting_delivery_interval =
        base::TimeDelta::FromMilliseconds(100);
  }

  // Always enable the HTTP cache.
  network_context_params->http_cache_enabled = true;
  network_context_params->cookie_manager_params =
      CreateCookieManagerParams(profile_, *cookie_settings_);

  // Configure on-disk storage for non-OTR profiles. OTR profiles just use
  // default behavior (in memory storage, default sizes).
  PrefService* prefs = profile_->GetPrefs();
  if (!in_memory) {
    // Configure the HTTP cache path and size.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(path, &base_cache_path);
    base::FilePath disk_cache_dir = prefs->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      base_cache_path = disk_cache_dir.Append(base_cache_path.BaseName());
    network_context_params->http_cache_path =
        base_cache_path.Append(chrome::kCacheDirname);
    network_context_params->http_cache_max_size =
        prefs->GetInteger(prefs::kDiskCacheSize);

    // Currently this just contains HttpServerProperties, but that will likely
    // change.
    network_context_params->http_server_properties_path =
        path.Append(chrome::kNetworkPersistentStateFilename);

    base::FilePath cookie_path = path;
    cookie_path = cookie_path.Append(chrome::kCookieFilename);
    network_context_params->cookie_path = cookie_path;

    if (relative_partition_path.empty()) {  // This is the main partition.
      network_context_params->restore_old_session_cookies =
          profile_->ShouldRestoreOldSessionCookies();
      network_context_params->persist_session_cookies =
          profile_->ShouldPersistSessionCookies();
    } else {
      // Copy behavior of ProfileImplIOData::InitializeAppRequestContext.
      network_context_params->restore_old_session_cookies = false;
      network_context_params->persist_session_cookies = false;
    }

    network_context_params->transport_security_persister_path = path;
  }

  // NOTE(mmenke): Keep these protocol handlers and
  // ProfileIOData::SetUpJobFactoryDefaultsForBuilder in sync with
  // ProfileIOData::IsHandledProtocol().
  // TODO(mmenke): Find a better way of handling tracking supported schemes.
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support = true;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  network_context_params->enable_certificate_reporting = true;
  network_context_params->enable_expect_ct_reporting = true;

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  if (!in_memory &&
      TrialComparisonCertVerifierController::MaybeAllowedForProfile(profile_)) {
    network::mojom::TrialComparisonCertVerifierConfigClientPtr config_client;
    auto config_client_request = mojo::MakeRequest(&config_client);

    network_context_params->trial_comparison_cert_verifier_params =
        network::mojom::TrialComparisonCertVerifierParams::New();

    if (!trial_comparison_cert_verifier_controller_) {
      trial_comparison_cert_verifier_controller_ =
          std::make_unique<TrialComparisonCertVerifierController>(profile_);
    }
    trial_comparison_cert_verifier_controller_->AddClient(
        std::move(config_client),
        mojo::MakeRequest(
            &network_context_params->trial_comparison_cert_verifier_params
                 ->report_client));
    network_context_params->trial_comparison_cert_verifier_params
        ->initial_allowed =
        trial_comparison_cert_verifier_controller_->IsAllowed();
    network_context_params->trial_comparison_cert_verifier_params
        ->config_client_request = std::move(config_client_request);
  }
#endif

  if (domain_reliability::DomainReliabilityServiceFactory::
          ShouldCreateService()) {
    network_context_params->enable_domain_reliability = true;
    network_context_params->domain_reliability_upload_reporter =
        domain_reliability::DomainReliabilityServiceFactory::
            kUploadReporterString;
    network_context_params->discard_domain_reliablity_uploads =
        g_discard_domain_reliability_uploads_for_testing
            ? *g_discard_domain_reliability_uploads_for_testing
            : !g_browser_process->local_state()->GetBoolean(
                  metrics::prefs::kMetricsReportingEnabled);
  }

  if (data_reduction_proxy::params::IsEnabledWithNetworkService()) {
    auto* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
    if (drp_settings) {
      network::mojom::CustomProxyConfigClientPtrInfo config_client_info;
      network_context_params->custom_proxy_config_client_request =
          mojo::MakeRequest(&config_client_info);
      drp_settings->SetCustomProxyConfigClient(std::move(config_client_info));
    }
  }

#if defined(OS_CHROMEOS)
  network_context_params->use_builtin_cert_verifier =
      using_builtin_cert_verifier_;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      network_context_params->username_hash = user->username_hash();
      network_context_params->nss_path = profile_->GetPath();
      if (policy::PolicyCertServiceFactory::CreateAndStartObservingForProfile(
              profile_)) {
        policy::PolicyCertService* service =
            policy::PolicyCertServiceFactory::GetForProfile(profile_);
        network_context_params->initial_additional_certificates =
            network::mojom::AdditionalCertificates::New();
        network_context_params->initial_additional_certificates
            ->all_certificates = service->all_server_and_authority_certs();
        network_context_params->initial_additional_certificates->trust_anchors =
            service->trust_anchors();
      }
    }
  }
#endif

  // Should be initialized with existing per-profile CORS access lists.
  network_context_params->cors_origin_access_list =
      profile_->GetSharedCorsOriginAccessList()
          ->GetOriginAccessList()
          .CreateCorsOriginAccessPatternsList();

  return network_context_params;
}

base::FilePath ProfileNetworkContextService::GetPartitionPath(
    const base::FilePath& relative_partition_path) {
  base::FilePath path = profile_->GetPath();
  if (!relative_partition_path.empty())
    path = path.Append(relative_partition_path);
  return path;
}

void ProfileNetworkContextService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES &&
      content_type != CONTENT_SETTINGS_TYPE_DEFAULT) {
    return;
  }

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &settings);
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](ContentSettingsForOneType settings,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetCookieManagerForBrowserProcess()
                          ->SetContentSettings(settings);
                    },
                    settings));
}
