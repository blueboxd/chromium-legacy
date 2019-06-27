// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/login/login_client_cert_usage_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/user_manager/user_type.h"
#include "net/base/net_errors.h"

class AccountId;

namespace base {
class DictionaryValue;
}  // namespace base

namespace net {
class CanonicalCookie;
}

namespace network {
class NSSTempCertsCacheChromeOS;
}

namespace chromeos {

class ActiveDirectoryPasswordChangeScreenHandler;
class Key;
class SamlPasswordAttributes;
class SigninScreenHandler;
class UserContext;

class GaiaView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"gaia-signin"};

  GaiaView() = default;
  virtual ~GaiaView() = default;

  // Decides whether an auth extension should be pre-loaded. If it should,
  // pre-loads it.
  virtual void MaybePreloadAuthExtension() = 0;

  virtual void DisableRestrictiveProxyCheckForTest() = 0;

  // Show the sign-in screen. Depending on internal state, the screen will
  // either be shown immediately or after an asynchronous clean-up process that
  // cleans DNS cache and cookies. If available, |account_id| is used for
  // prefilling information.
  virtual void ShowGaiaAsync(const AccountId& account_id) = 0;

  // Show sign-in screen for the given credentials. |services| is a list of
  // services returned by userInfo call as JSON array. Should be an empty array
  // for a regular user: "[]".
  virtual void ShowSigninScreenForTest(const std::string& username,
                                       const std::string& password,
                                       const std::string& services) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GaiaView);
};

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler : public BaseScreenHandler,
                          public GaiaView,
                          public NetworkPortalDetector::Observer {
 public:
  using TView = GaiaView;

  // The possible modes that the Gaia signin screen can be in.
  enum GaiaScreenMode {
    // Default Gaia authentication will be used.
    GAIA_SCREEN_MODE_DEFAULT = 0,

    // Gaia offline mode will be used.
    GAIA_SCREEN_MODE_OFFLINE = 1,

    // An interstitial page will be used before SAML redirection.
    GAIA_SCREEN_MODE_SAML_INTERSTITIAL = 2,

    // Offline UI for Active Directory authentication.
    GAIA_SCREEN_MODE_AD = 3,
  };

  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR
  };

  GaiaScreenHandler(
      JSCallsContainer* js_calls_container,
      CoreOobeView* core_oobe_view,
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ActiveDirectoryPasswordChangeScreenHandler*
          active_directory_password_change_screen_handler);
  ~GaiaScreenHandler() override;

  // GaiaView:
  void MaybePreloadAuthExtension() override;
  void DisableRestrictiveProxyCheckForTest() override;
  void ShowGaiaAsync(const AccountId& account_id) override;
  void ShowSigninScreenForTest(const std::string& username,
                               const std::string& password,
                               const std::string& services) override;

  // Returns true if offline login mode was either required, or reported by the
  // WebUI (i.e. WebUI mignt not have completed transition to the new mode).
  bool IsOfflineLoginActive() const;

 private:
  // TODO (xiaoyinh): remove this dependency.
  friend class SigninScreenHandler;

  struct GaiaContext;

  void LoadGaia(const GaiaContext& context);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartition(const GaiaContext& context,
                             const std::string& partition_name);

  // Called after the GAPS cookie, if present, is added to the cookie store.
  void OnSetCookieForLoadGaiaWithPartition(
      const GaiaContext& context,
      const std::string& partition_name,
      net::CanonicalCookie::CookieInclusionStatus status);

  // Callback that loads GAIA after version and stat consent information has
  // been retrieved.
  void LoadGaiaWithPartitionAndVersionAndConsent(
      const GaiaContext& context,
      const std::string& partition_name,
      const std::string* platform_version,
      const bool* collect_stats_consent);

  // Sends request to reload Gaia. If |force_reload| is true, request
  // will be sent in any case, otherwise it will be sent only when Gaia is
  // not loading right now.
  void ReloadGaia(bool force_reload);

  // Turns offline idle detection on or off. Idle detection should only be on if
  // we're using the offline login page but the device is online.
  void MonitorOfflineIdle(bool is_online);

  // Show error UI at the end of GAIA flow when user is not whitelisted.
  void ShowWhitelistCheckFailedError();

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // NetworkPortalDetector::Observer implementation.
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // WebUI message handlers.
  void HandleWebviewLoadAborted(const std::string& error_reason_str);
  void HandleCompleteAuthentication(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      bool using_saml,
      const ::login::StringList& services,
      const base::DictionaryValue* password_attributes);
  void OnGetCookiesForCompleteAuthentication(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      bool using_saml,
      const ::login::StringList& services,
      const SamlPasswordAttributes& password_attributes,
      const std::vector<net::CanonicalCookie>& cookies,
      const net::CookieStatusList& excluded_cookies);
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);

  void HandleCompleteAdAuthentication(const std::string& username,
                                      const std::string& password);

  void HandleCancelActiveDirectoryAuth();

  void HandleUsingSAMLAPI();
  void HandleScrapedPasswordCount(int password_count);
  void HandleScrapedPasswordVerificationFailed();

  void HandleGaiaUIReady();

  void HandleIdentifierEntered(const std::string& account_identifier);

  void HandleAuthExtensionLoaded();
  void HandleUpdateOobeDialogSize(int width, int height);
  void HandleHideOobeDialog();
  void HandleShowAddUser(const base::ListValue* args);
  void HandleGetIsSamlUserPasswordless(const std::string& callback_id,
                                       const std::string& typed_email,
                                       const std::string& gaia_id);
  void HandleUpdateSigninUIState(int state);

  // Allows WebUI to control the login shelf's guest button visibility during
  // OOBE.
  void HandleShowGuestInOobe(bool show);

  void OnShowAddUser();

  // Really handles the complete login message.
  void DoCompleteLogin(const std::string& gaia_id,
                       const std::string& typed_email,
                       const std::string& password,
                       bool using_saml,
                       const SamlPasswordAttributes& password_attributes);

  // Fill GAIA user name.
  void set_populated_email(const std::string& populated_email) {
    populated_email_ = populated_email;
  }

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(const base::Closure& on_clear_callback);
  void OnCookiesCleared(const base::Closure& on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Callback for AuthPolicyClient.
  void DoAdAuth(const std::string& username,
                const Key& key,
                authpolicy::ErrorType error,
                const authpolicy::ActiveDirectoryAccountInfo& account_info);

  // Attempts login for test.
  void SubmitLoginFormForTest();

  // Updates the member variable and UMA histogram indicating whether the
  // principals API was used during SAML login.
  void SetSAMLPrincipalsAPIUsed(bool api_used);

  // Cancels the request to show the sign-in screen while the asynchronous
  // clean-up process that precedes the screen showing is in progress.
  void CancelShowGaiaAsync();

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |offline| is true when
  // offline version of the extension should be used.
  void LoadAuthExtension(bool force, bool offline);

  // TODO (antrim@): GaiaScreenHandler should implement
  // NetworkStateInformer::Observer.
  void UpdateState(NetworkError::ErrorReason reason);

  // TODO (antrim@): remove this dependency.
  void set_signin_screen_handler(SigninScreenHandler* handler) {
    signin_screen_handler_ = handler;
  }

  // Are we on a restrictive proxy?
  bool IsRestrictiveProxy() const;

  // Returns temporary unused device Id.
  std::string GetTemporaryDeviceId();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

  // Returns user canonical e-mail. Finds already used account alias, if
  // user has already signed in.
  AccountId GetAccountId(const std::string& authenticated_email,
                         const std::string& id,
                         const AccountType& account_type) const;

  // Records whether WebUI is currently in offline mode.
  void SetOfflineLoginIsActive(bool is_active);

  // Builds the UserContext with the information from the given Gaia user
  // sign-in. On failure, returns false and sets |error_message|.
  bool BuildUserContextForGaiaSignIn(
      user_manager::UserType user_type,
      const AccountId& account_id,
      bool using_saml,
      const std::string& password,
      const std::string& auth_code,
      const std::string& gaps_cookie,
      const SamlPasswordAttributes& password_attributes,
      UserContext* user_context,
      std::string* error_message);

  // Current state of Gaia frame.
  FrameState frame_state_ = FRAME_STATE_UNKNOWN;

  // Latest Gaia frame error.
  net::Error frame_error_ = net::OK;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  CoreOobeView* core_oobe_view_ = nullptr;

  ActiveDirectoryPasswordChangeScreenHandler*
      active_directory_password_change_screen_handler_ = nullptr;

  // Email to pre-populate with.
  std::string populated_email_;

  // Whether the handler has been initialized.
  bool initialized_ = false;

  // True if dns cache cleanup is done.
  bool dns_cleared_ = false;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_ = false;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_ = false;

  // If true, the sign-in screen will be shown when DNS cache and cookie
  // clean-up finish, and the handler is initialized (i.e. the web UI is ready).
  bool show_when_ready_ = false;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_ = false;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // If the user authenticated via SAML, this indicates whether the principals
  // API was used.
  // TODO(emaxx): This is also currently set when the user authenticated via
  // Gaia, since Gaia uses the same API for passing the password to Chrome.
  // Either fix this behavior, or change the naming and the comments to reflect
  // it.
  bool using_saml_api_ = false;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;
  // Test result of userInfo.
  std::string test_services_;
  bool test_expects_complete_login_ = false;

  // True if proxy doesn't allow access to google.com/generate_204.
  NetworkPortalDetector::CaptivePortalStatus captive_portal_status_ =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;

  std::unique_ptr<NetworkPortalDetector> network_portal_detector_;
  bool disable_restrictive_proxy_check_for_test_ = false;

  // Non-owning ptr to SigninScreenHandler instance. Should not be used
  // in dtor.
  // TODO (antrim@): GaiaScreenHandler shouldn't communicate with
  // signin_screen_handler directly.
  SigninScreenHandler* signin_screen_handler_ = nullptr;

  // True if WebUI is currently displaying offline GAIA.
  bool offline_login_is_active_ = false;

  // True if the authentication extension is still loading.
  bool auth_extension_being_loaded_ = false;

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to
  // authenticate users against Active Directory server.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  // Makes untrusted authority certificates from device policy available for
  // client certificate discovery.
  std::unique_ptr<network::NSSTempCertsCacheChromeOS>
      untrusted_authority_certs_cache_;

  // The type of Gaia page to show.
  GaiaScreenMode screen_mode_ = GAIA_SCREEN_MODE_DEFAULT;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
