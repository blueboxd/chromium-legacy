// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/enterprise/reporting/report_generator.h"
#include "chrome/browser/enterprise/reporting/report_scheduler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage_linux.h"
#include "chrome/browser/policy/browser_dm_token_storage_mac.h"
#include "chrome/browser/policy/browser_dm_token_storage_win.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_helper.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/policy/core/common/cloud/chrome_browser_cloud_management_metrics.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_util.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/base_paths_win.h"
#include "chrome/install_static/install_modes.h"
#else
#include "chrome/common/chrome_switches.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/app_controller_mac.h"
#endif

namespace policy {

namespace {

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::FilePath::StringPieceType kCachedPolicyDirname =
    FILE_PATH_LITERAL("Policies");
constexpr base::FilePath::StringPieceType kCachedPolicyFilename =
    FILE_PATH_LITERAL("PolicyFetchResponse");
#endif

void RecordEnrollmentResult(
    ChromeBrowserCloudManagementEnrollmentResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result", result);
}

// Read the kCloudPolicyOverridesPlatformPolicy from platform provider directly
// because the local_state is not ready when the
// MachineLevelUserCloudPolicyManager is created.
bool DoesCloudPolicyHasPriority(
    ConfigurationPolicyProvider* platform_provider) {
  if (!platform_provider)
    return false;
  const auto* entry =
      platform_provider->policies()
          .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .Get(key::kCloudPolicyOverridesPlatformPolicy);
  if (!entry || entry->scope == POLICY_SCOPE_USER ||
      entry->level == POLICY_LEVEL_RECOMMENDED)
    return false;

  return entry->value()->is_bool() && entry->value()->GetBool();
}

}  // namespace

const base::FilePath::CharType
    ChromeBrowserCloudManagementController::kPolicyDir[] =
        FILE_PATH_LITERAL("Policy");

bool ChromeBrowserCloudManagementController::IsEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableChromeBrowserCloudManagement);
#endif
}

// A helper class to make the appropriate calls into the device account
// initializer and manage the ChromeBrowserCloudManagementRegistrar callback's
// lifetime.
class MachineLevelDeviceAccountInitializerHelper
    : public DeviceAccountInitializer::Delegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  // |policy_client| should be registered and outlive this object.
  MachineLevelDeviceAccountInitializerHelper(
      policy::CloudPolicyClient* policy_client,
      Callback callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : policy_client_(std::move(policy_client)),
        callback_(std::move(callback)),
        url_loader_factory_(url_loader_factory) {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations));

    DCHECK(url_loader_factory_);

    device_account_initializer_ =
        std::make_unique<DeviceAccountInitializer>(policy_client_, this);
    device_account_initializer_->FetchToken();
  }

  MachineLevelDeviceAccountInitializerHelper& operator=(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&&) = delete;

  ~MachineLevelDeviceAccountInitializerHelper() override = default;

  // DeviceAccountInitializer::Delegate:
  void OnDeviceAccountTokenFetched(bool empty_token) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    if (empty_token) {
      // Not being able to obtain a token isn't a showstopper for machine
      // level policies: the browser will fallback to fetching policies on a
      // regular schedule and won't support remote commands. Getting a refresh
      // token will be reattempted on the next successful policy fetch.
      std::move(callback_).Run(false);
      return;
    }

    device_account_initializer_->StoreToken();
  }

  void OnDeviceAccountTokenStored() override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(true);
  }

  void OnDeviceAccountTokenError(EnrollmentStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(false);
  }

  void OnDeviceAccountClientError(DeviceManagementStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(false);
  }

  enterprise_management::DeviceServiceApiAccessRequest::DeviceType
  GetRobotAuthCodeDeviceType() override {
    return enterprise_management::DeviceServiceApiAccessRequest::CHROME_BROWSER;
  }

  std::set<std::string> GetRobotOAuthScopes() override {
    return {
        GaiaConstants::kOAuthWrapBridgeUserInfoScope,
        GaiaConstants::kFCMOAuthScope,
    };
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

  policy::CloudPolicyClient* policy_client_;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;
  Callback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

ChromeBrowserCloudManagementController::ChromeBrowserCloudManagementController()
    : gaia_url_loader_factory_(nullptr) {
  std::unique_ptr<BrowserDMTokenStorage::Delegate> storage_delegate;

#if defined(OS_WIN)
  storage_delegate = std::make_unique<BrowserDMTokenStorageWin>();
#elif defined(OS_MACOSX)
  storage_delegate = std::make_unique<BrowserDMTokenStorageMac>();
#elif defined(OS_LINUX)
  storage_delegate = std::make_unique<BrowserDMTokenStorageLinux>();
#else
  NOT_REACHED();
#endif

  BrowserDMTokenStorage::SetDelegate(std::move(storage_delegate));
}

ChromeBrowserCloudManagementController::
    ~ChromeBrowserCloudManagementController() {
  if (policy_fetcher_)
    policy_fetcher_->RemoveClientObserver(this);
  if (cloud_policy_client_)
    cloud_policy_client_->RemoveObserver(this);
}

// static
std::unique_ptr<MachineLevelUserCloudPolicyManager>
ChromeBrowserCloudManagementController::CreatePolicyManager(
    ConfigurationPolicyProvider* platform_provider) {
  if (!IsEnabled())
    return nullptr;

  std::string enrollment_token =
      BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  DMToken dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (dm_token.is_empty())
    VLOG(1) << "DM token = none";
  else if (dm_token.is_invalid())
    VLOG(1) << "DM token = invalid";
  else if (dm_token.is_valid())
    VLOG(1) << "DM token = from persistence";

  VLOG(1) << "Enrollment token = " << enrollment_token;
  VLOG(1) << "Client ID = " << client_id;

  // Don't create the policy manager if the DM token is explicitly invalid or if
  // both tokens are empty.
  if (dm_token.is_invalid() ||
      (enrollment_token.empty() && dm_token.is_empty())) {
    return nullptr;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return nullptr;

  DVLOG(1) << "Creating machine level user cloud policy manager";

  bool cloud_policy_has_priority =
      DoesCloudPolicyHasPriority(platform_provider);
  if (cloud_policy_has_priority) {
    DVLOG(1) << "Cloud policies are now overriding platform policies with "
                "machine scope.";
  }

  base::FilePath policy_dir =
      user_data_dir.Append(ChromeBrowserCloudManagementController::kPolicyDir);

  base::FilePath external_policy_path;
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &external_policy_path);

  external_policy_path =
      external_policy_path.Append(install_static::kCompanyPathName)
          .Append(kCachedPolicyDirname)
          .AppendASCII(
              policy::dm_protocol::kChromeMachineLevelUserCloudPolicyTypeBase64)
          .Append(kCachedPolicyFilename);
#endif

  std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
      MachineLevelUserCloudPolicyStore::Create(
          dm_token, client_id, external_policy_path, policy_dir,
          cloud_policy_has_priority,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               // Block shutdown to make sure the policy cache update is always
               // finished.
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  return std::make_unique<MachineLevelUserCloudPolicyManager>(
      std::move(policy_store), nullptr, policy_dir,
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&content::GetNetworkConnectionTracker));
}

void ChromeBrowserCloudManagementController::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!IsEnabled())
    return;

  if (base::FeatureList::IsEnabled(
          policy::features::kCBCMPolicyInvalidations)) {
    DeviceOAuth2TokenServiceFactory::Initialize(url_loader_factory,
                                                local_state);
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &ChromeBrowserCloudManagementController::CreateReportSchedulerAsync,
          base::Unretained(this), base::ThreadTaskRunnerHandle::Get()));

  MachineLevelUserCloudPolicyManager* policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();

  if (!policy_manager)
    return;
  // If there exists an enrollment token, then there are three states:
  //   1/ There also exists a valid DM token.  This machine is already
  //      registered, so the next step is to fetch policies.
  //   2/ There is no DM token.  In this case the machine is not already
  //      registered and needs to request a DM token.
  //   3/ The also exists an invalid DM token.  Do not fetch policies or try to
  //      request a DM token in that case.
  std::string enrollment_token;
  std::string client_id;
  DMToken dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();

  if (dm_token.is_invalid())
    return;

  if (dm_token.is_valid()) {
    policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
        policy_manager, local_state, device_management_service,
        url_loader_factory);
    policy_fetcher_->AddClientObserver(this);
    return;
  }

  if (!GetEnrollmentTokenAndClientId(&enrollment_token, &client_id))
    return;

  DCHECK(!enrollment_token.empty());
  DCHECK(!client_id.empty());

  cloud_management_registrar_ =
      std::make_unique<ChromeBrowserCloudManagementRegistrar>(
          device_management_service, url_loader_factory);
  policy_fetcher_ = std::make_unique<MachineLevelUserCloudPolicyFetcher>(
      policy_manager, local_state, device_management_service,
      url_loader_factory);
  policy_fetcher_->AddClientObserver(this);

  if (dm_token.is_empty()) {
    cloud_management_register_watcher_ =
        std::make_unique<ChromeBrowserCloudManagementRegisterWatcher>(this);

    enrollment_start_time_ = base::Time::Now();

    // Not registered already, so do it now.
    cloud_management_registrar_->RegisterForCloudManagementWithEnrollmentToken(
        enrollment_token, client_id,
        base::Bind(&ChromeBrowserCloudManagementController::
                       RegisterForCloudManagementWithEnrollmentTokenCallback,
                   base::Unretained(this)));
    // On Windows, if Chrome is installed on the user level, we can't store the
    // DM token in the registry at the end of enrollment. Hence Chrome needs to
    // re-enroll every launch.
    // Based on the UMA metrics
    // Enterprise.MachineLevelUserCloudPolicyEnrollment.InstallLevel_Win,
    // the number of user-level enrollment is very low
    // compare to the total CBCM users. In additional to that, devices are now
    // mostly enrolled with Google Update on Windows. Based on that, we won't do
    // anything special for user-level install enrollment.
  }
}

bool ChromeBrowserCloudManagementController::
    WaitUntilPolicyEnrollmentFinished() {
  if (cloud_management_register_watcher_) {
    switch (cloud_management_register_watcher_
                ->WaitUntilCloudPolicyEnrollmentFinished()) {
      case RegisterResult::kNoEnrollmentNeeded:
      case RegisterResult::kEnrollmentSuccessBeforeDialogDisplayed:
      case RegisterResult::kEnrollmentFailedSilentlyBeforeDialogDisplayed:
        return true;
      case RegisterResult::kEnrollmentSuccess:
      case RegisterResult::kEnrollmentFailedSilently:
#if defined(OS_MACOSX)
        app_controller_mac::EnterpriseStartupDialogClosed();
#endif
        return true;
      case RegisterResult::kRestartDueToFailure:
        chrome::AttemptRestart();
        return false;
      case RegisterResult::kQuitDueToFailure:
        chrome::AttemptExit();
        return false;
    }
  }
  return true;
}

void ChromeBrowserCloudManagementController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeBrowserCloudManagementController::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ChromeBrowserCloudManagementController::
    IsEnterpriseStartupDialogShowing() {
  return cloud_management_register_watcher_ &&
         cloud_management_register_watcher_->IsDialogShowing();
}

void ChromeBrowserCloudManagementController::UnenrollBrowser() {
  // Invalidate DM token in storage.
  BrowserDMTokenStorage::Get()->InvalidateDMToken(base::BindOnce(
      &ChromeBrowserCloudManagementController::InvalidateDMTokenCallback,
      base::Unretained(this)));
}

void ChromeBrowserCloudManagementController::InvalidatePolicies() {
  // Reset policies.
  if (policy_fetcher_) {
    policy_fetcher_->RemoveClientObserver(this);
    policy_fetcher_->Disconnect();
  }

  // This causes the scheduler to stop refreshing itself since the DM token is
  // no longer valid.
  if (report_scheduler_)
    report_scheduler_->OnDMTokenUpdated();
}

void ChromeBrowserCloudManagementController::InvalidateDMTokenCallback(
    bool success) {
  UMA_HISTOGRAM_BOOLEAN(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.UnenrollSuccess",
      success);
  if (success) {
    DVLOG(1) << "Successfully invalidated the DM token";
    InvalidatePolicies();
  } else {
    DVLOG(1) << "Failed to invalidate the DM token";
  }
  NotifyBrowserUnenrolled(success);
}

void ChromeBrowserCloudManagementController::OnPolicyFetched(
    CloudPolicyClient* client) {
  // Ignored.
}

void ChromeBrowserCloudManagementController::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  // Ignored.
}

void ChromeBrowserCloudManagementController::OnClientError(
    CloudPolicyClient* client) {
  // DM_STATUS_SERVICE_DEVICE_NOT_FOUND being the last status implies the
  // browser has been unenrolled.
  if (client->status() == DM_STATUS_SERVICE_DEVICE_NOT_FOUND)
    UnenrollBrowser();
}

void ChromeBrowserCloudManagementController::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  if (!base::FeatureList::IsEnabled(
          policy::features::kCBCMPolicyInvalidations)) {
    return;
  }

  // No need to get a refresh token if there is one present already.
  if (!DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable()) {
    // If this feature is enabled, we need to ensure the device service
    // account is initialized and fetch auth codes to exchange for a refresh
    // token. Creating this object starts that process and the callback will
    // be called from it whether it succeeds or not.
    DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
        account_email);
    account_initializer_helper_ =
        std::make_unique<MachineLevelDeviceAccountInitializerHelper>(
            std::move(client),
            base::BindOnce(
                &ChromeBrowserCloudManagementController::AccountInitCallback,
                base::Unretained(this), account_email),
            gaia_url_loader_factory_
                ? gaia_url_loader_factory_
                : g_browser_process->system_network_context_manager()
                      ->GetSharedURLLoaderFactory());
  } else if (!policy_invalidator_) {
    // There's already a refresh token available but no |policy_invalidator_|
    // which means this is browser startup and the refresh token was retrieved
    // from local storage. It's OK to start invalidations now.
    StartInvalidations();
  }
}

void ChromeBrowserCloudManagementController::ShutDown() {
  if (policy_invalidator_)
    policy_invalidator_->Shutdown();
  if (report_scheduler_)
    report_scheduler_.reset();
}

void ChromeBrowserCloudManagementController::NotifyPolicyRegisterFinished(
    bool succeeded) {
  for (auto& observer : observers_) {
    observer.OnPolicyRegisterFinished(succeeded);
  }
}

void ChromeBrowserCloudManagementController::NotifyBrowserUnenrolled(
    bool succeeded) {
  for (auto& observer : observers_)
    observer.OnBrowserUnenrolled(succeeded);
}

void ChromeBrowserCloudManagementController::NotifyCloudReportingLaunched() {
  for (auto& observer : observers_) {
    observer.OnCloudReportingLaunched();
  }
}

bool ChromeBrowserCloudManagementController::GetEnrollmentTokenAndClientId(
    std::string* enrollment_token,
    std::string* client_id) {
  *client_id = BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id->empty())
    return false;

  *enrollment_token = BrowserDMTokenStorage::Get()->RetrieveEnrollmentToken();
  return !enrollment_token->empty();
}

void ChromeBrowserCloudManagementController::
    RegisterForCloudManagementWithEnrollmentTokenCallback(
        const std::string& dm_token,
        const std::string& client_id) {
  base::TimeDelta enrollment_time = base::Time::Now() - enrollment_start_time_;

  if (dm_token.empty()) {
    VLOG(1) << "No DM token returned from browser registration.";
    RecordEnrollmentResult(
        ChromeBrowserCloudManagementEnrollmentResult::kFailedToFetch);
    UMA_HISTOGRAM_TIMES(
        "Enterprise.MachineLevelUserCloudPolicyEnrollment.RequestFailureTime",
        enrollment_time);
    MachineLevelUserCloudPolicyManager* policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    if (policy_manager)
      policy_manager->store()->InitWithoutToken();
    NotifyPolicyRegisterFinished(false);
    return;
  }

  VLOG(1) << "DM token retrieved from server.";

  UMA_HISTOGRAM_TIMES(
      "Enterprise.MachineLevelUserCloudPolicyEnrollment.RequestSuccessTime",
      enrollment_time);

  // TODO(alito): Log failures to store the DM token. Should we try again later?
  BrowserDMTokenStorage::Get()->StoreDMToken(
      dm_token, base::BindOnce([](bool success) {
        if (!success) {
          DVLOG(1) << "Failed to store the DM token";
          RecordEnrollmentResult(
              ChromeBrowserCloudManagementEnrollmentResult::kFailedToStore);
        } else {
          DVLOG(1) << "Successfully stored the DM token";
          RecordEnrollmentResult(
              ChromeBrowserCloudManagementEnrollmentResult::kSuccess);
        }
      }));

  // Start fetching policies.
  VLOG(1) << "Fetch policy after enrollment.";
  policy_fetcher_->SetupRegistrationAndFetchPolicy(
      BrowserDMTokenStorage::Get()->RetrieveDMToken(), client_id);
  if (report_scheduler_) {
    report_scheduler_->OnDMTokenUpdated();
  }

  NotifyPolicyRegisterFinished(true);
}

void ChromeBrowserCloudManagementController::CreateReportSchedulerAsync(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeBrowserCloudManagementController::CreateReportScheduler,
          base::Unretained(this)));
}

void ChromeBrowserCloudManagementController::CreateReportScheduler() {
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      g_browser_process->browser_policy_connector()
          ->device_management_service(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      CloudPolicyClient::DeviceDMTokenCallback());
  cloud_policy_client_->AddObserver(this);
  auto generator = std::make_unique<enterprise_reporting::ReportGenerator>();
  report_scheduler_ = std::make_unique<enterprise_reporting::ReportScheduler>(
      cloud_policy_client_.get(), std::move(generator));

  NotifyCloudReportingLaunched();
}

void ChromeBrowserCloudManagementController::StartInvalidations() {
  DCHECK(
      base::FeatureList::IsEnabled(policy::features::kCBCMPolicyInvalidations));

  identity_provider_ = std::make_unique<DeviceIdentityProvider>(
      DeviceOAuth2TokenServiceFactory::Get());
  device_instance_id_driver_ = std::make_unique<instance_id::InstanceIDDriver>(
      g_browser_process->gcm_driver());

  invalidation_service_ =
      std::make_unique<invalidation::FCMInvalidationService>(
          identity_provider_.get(),
          base::BindRepeating(&syncer::FCMNetworkHandler::Create,
                              g_browser_process->gcm_driver(),
                              device_instance_id_driver_.get()),
          base::BindRepeating(
              &syncer::PerUserTopicSubscriptionManager::Create,
              identity_provider_.get(), g_browser_process->local_state(),
              base::RetainedRef(
                  g_browser_process->shared_url_loader_factory())),
          device_instance_id_driver_.get(), g_browser_process->local_state(),
          policy::kPolicyFCMInvalidationSenderID);
  invalidation_service_->Init();

  policy_invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      PolicyInvalidationScope::kCBCM,
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager()
          ->core(),
      base::ThreadTaskRunnerHandle::Get(), base::DefaultClock::GetInstance(),
      0 /* highest_handled_invalidation_version */);
  policy_invalidator_->Initialize(invalidation_service_.get());
}

void ChromeBrowserCloudManagementController::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  gaia_url_loader_factory_ = url_loader_factory;
}

void ChromeBrowserCloudManagementController::AccountInitCallback(
    const std::string& account_email,
    bool success) {
  account_initializer_helper_.reset();
  if (success)
    StartInvalidations();
}

}  // namespace policy
