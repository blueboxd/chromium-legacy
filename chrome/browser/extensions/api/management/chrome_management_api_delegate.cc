// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#endif

namespace {

using InstallWebAppCallback =
    extensions::ManagementAPIDelegate::InstallWebAppCallback;
using InstallWebAppResult =
    extensions::ManagementAPIDelegate::InstallWebAppResult;

class ManagementSetEnabledFunctionInstallPromptDelegate
    : public extensions::InstallPromptDelegate {
 public:
  ManagementSetEnabledFunctionInstallPromptDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      const base::Callback<void(bool)>& callback)
      : install_prompt_(new ExtensionInstallPrompt(web_contents)),
        callback_(callback) {
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
            browser_context, extension);
    install_prompt_->ShowDialog(
        base::Bind(&ManagementSetEnabledFunctionInstallPromptDelegate::
                       OnInstallPromptDone,
                   weak_factory_.GetWeakPtr()),
        extension, nullptr,
        std::make_unique<ExtensionInstallPrompt::Prompt>(type),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }
  ~ManagementSetEnabledFunctionInstallPromptDelegate() override {}

 private:
  void OnInstallPromptDone(ExtensionInstallPrompt::Result result) {
    std::move(callback_).Run(result ==
                             ExtensionInstallPrompt::Result::ACCEPTED);
  }

  // Used for prompting to re-enable items with permissions escalation updates.
  std::unique_ptr<ExtensionInstallPrompt> install_prompt_;

  base::Callback<void(bool)> callback_;

  base::WeakPtrFactory<ManagementSetEnabledFunctionInstallPromptDelegate>
      weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ManagementSetEnabledFunctionInstallPromptDelegate);
};

class ManagementUninstallFunctionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate,
      public extensions::UninstallDialogDelegate {
 public:
  ManagementUninstallFunctionUninstallDialogDelegate(
      extensions::ManagementUninstallFunctionBase* function,
      const extensions::Extension* target_extension,
      bool show_programmatic_uninstall_ui)
      : function_(function) {
    ChromeExtensionFunctionDetails details(function);
    extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
        details.GetProfile(), details.GetNativeWindowForUI(), this);
    bool uninstall_from_webstore =
        function->extension() &&
        function->extension()->id() == extensions::kWebStoreAppId;
    extensions::UninstallSource source;
    extensions::UninstallReason reason;
    if (uninstall_from_webstore) {
      source = extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE;
      reason = extensions::UNINSTALL_REASON_CHROME_WEBSTORE;
    } else if (function->source_context_type() ==
               extensions::Feature::WEBUI_CONTEXT) {
      source = extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE;
      // TODO: Update this to a new reason; it shouldn't be lumped in with
      // other uninstalls if it's from the chrome://extensions page.
      reason = extensions::UNINSTALL_REASON_MANAGEMENT_API;
    } else {
      source = extensions::UNINSTALL_SOURCE_EXTENSION;
      reason = extensions::UNINSTALL_REASON_MANAGEMENT_API;
    }
    if (show_programmatic_uninstall_ui) {
      extension_uninstall_dialog_->ConfirmUninstallByExtension(
          target_extension, function->extension(), reason, source);
    } else {
      extension_uninstall_dialog_->ConfirmUninstall(target_extension, reason,
                                                    source);
    }
  }

  ~ManagementUninstallFunctionUninstallDialogDelegate() override {}

  // ExtensionUninstallDialog::Delegate implementation.
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error) override {
    function_->OnExtensionUninstallDialogClosed(did_start_uninstall, error);
  }

 private:
  extensions::ManagementUninstallFunctionBase* function_;
  std::unique_ptr<extensions::ExtensionUninstallDialog>
      extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ManagementUninstallFunctionUninstallDialogDelegate);
};

void OnGenerateAppForLinkCompleted(
    extensions::ManagementGenerateAppForLinkFunction* function,
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  const bool install_success = code == web_app::InstallResultCode::kSuccess;
  function->FinishCreateWebApp(app_id, install_success);
}

class ChromeAppForLinkDelegate : public extensions::AppForLinkDelegate {
 public:
  ChromeAppForLinkDelegate() {}
  ~ChromeAppForLinkDelegate() override {}

  void OnFaviconForApp(
      extensions::ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url,
      const favicon_base::FaviconImageResult& image_result) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->title = base::UTF8ToUTF16(title);
    web_app_info->app_url = launch_url;

    if (!image_result.image.IsEmpty()) {
      WebApplicationInfo::IconInfo icon;
      icon.data = image_result.image.AsBitmap();
      icon.width = icon.data.width();
      icon.height = icon.data.height();
      web_app_info->icons.push_back(icon);
    }

    auto* provider = web_app::WebAppProviderBase::GetProviderBase(
        Profile::FromBrowserContext(context));
    DCHECK(provider);

    provider->install_manager().InstallWebAppFromInfo(
        std::move(web_app_info), /*no_network_install=*/false,
        WebappInstallSource::MANAGEMENT_API,
        base::BindOnce(OnGenerateAppForLinkCompleted,
                       base::RetainedRef(function)));
  }

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAppForLinkDelegate);
};

void OnWebAppInstallCompleted(InstallWebAppCallback callback,
                              const web_app::AppId& app_id,
                              web_app::InstallResultCode code) {
  InstallWebAppResult result;
  // TODO(loyso): Update this when more of the web_app::InstallResultCodes are
  // actually set.
  switch (code) {
    case web_app::InstallResultCode::kSuccess:
      result = InstallWebAppResult::kSuccess;
      break;
    default:
      result = InstallWebAppResult::kUnknownError;
  }
  std::move(callback).Run(result);
}

void OnDidInstallWebAppInstallableCheck(
    Profile* profile,
    InstallWebAppCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    bool is_installable) {
  if (!is_installable) {
    std::move(callback).Run(InstallWebAppResult::kInvalidWebApp);
    return;
  }

  content::WebContents* containing_contents = web_contents.get();
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  chrome::AddWebContents(displayer.browser(), nullptr, std::move(web_contents),
                         WindowOpenDisposition::NEW_FOREGROUND_TAB,
                         gfx::Rect());
  web_app::CreateWebAppFromManifest(
      containing_contents, WebappInstallSource::MANAGEMENT_API,
      base::BindOnce(&OnWebAppInstallCompleted, std::move(callback)));
}

}  // namespace

ChromeManagementAPIDelegate::ChromeManagementAPIDelegate() {
}

ChromeManagementAPIDelegate::~ChromeManagementAPIDelegate() {
}

void ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  // Look at prefs to find the right launch container.
  // If the user has not set a preference, the default launch value will be
  // returned.
  extensions::LaunchContainer launch_container =
      GetLaunchContainer(extensions::ExtensionPrefs::Get(context), extension);
  OpenApplication(AppLaunchParams(
      Profile::FromBrowserContext(context), extension->id(), launch_container,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      extensions::AppLaunchSource::kSourceManagementApi));

#if defined(OS_CHROMEOS)
  chromeos::DemoSession::RecordAppLaunchSourceIfInDemoMode(
      chromeos::DemoSession::AppLaunchSource::kExtensionApi);
#endif

  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_EXTENSION_API,
                                  extension->GetType());
}

GURL ChromeManagementAPIDelegate::GetFullLaunchURL(
    const extensions::Extension* extension) const {
  return extensions::AppLaunchInfo::GetFullLaunchURL(extension);
}

extensions::LaunchType ChromeManagementAPIDelegate::GetLaunchType(
    const extensions::ExtensionPrefs* prefs,
    const extensions::Extension* extension) const {
  return extensions::GetLaunchType(prefs, extension);
}

void ChromeManagementAPIDelegate::
    GetPermissionWarningsByManifestFunctionDelegate(
        extensions::ManagementGetPermissionWarningsByManifestFunction* function,
        const std::string& manifest_str) const {
  data_decoder::SafeJsonParser::Parse(
      content::GetSystemConnector(), manifest_str,
      base::BindOnce(
          &extensions::ManagementGetPermissionWarningsByManifestFunction::
              OnParseSuccess,
          function),
      base::BindOnce(
          &extensions::ManagementGetPermissionWarningsByManifestFunction::
              OnParseFailure,
          function));
}

std::unique_ptr<extensions::InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    const base::Callback<void(bool)>& callback) const {
  return std::unique_ptr<ManagementSetEnabledFunctionInstallPromptDelegate>(
      new ManagementSetEnabledFunctionInstallPromptDelegate(
          web_contents, browser_context, extension, callback));
}

std::unique_ptr<extensions::UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    extensions::ManagementUninstallFunctionBase* function,
    const extensions::Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  return std::unique_ptr<extensions::UninstallDialogDelegate>(
      new ManagementUninstallFunctionUninstallDialogDelegate(
          function, target_extension, show_programmatic_uninstall_ui));
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    extensions::ManagementCreateAppShortcutFunction* function,
    const extensions::Extension* extension,
    std::string* error) const {
  Browser* browser = chrome::FindBrowserWithProfile(
      Profile::FromBrowserContext(function->browser_context()));
  if (!browser) {
    // Shouldn't happen if we have user gesture.
    *error = extension_management_api_constants::kNoBrowserToCreateShortcut;
    return false;
  }

  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::Bind(&extensions::ManagementCreateAppShortcutFunction::
                     OnCloseShortcutPrompt,
                 function));

  return true;
}

std::unique_ptr<extensions::AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    extensions::ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(Profile::FromBrowserContext(context),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);

  ChromeAppForLinkDelegate* delegate = new ChromeAppForLinkDelegate;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::Bind(&ChromeAppForLinkDelegate::OnFaviconForApp,
                 base::Unretained(delegate), base::RetainedRef(function),
                 context, title, launch_url),
      &delegate->cancelable_task_tracker_);

  return std::unique_ptr<extensions::AppForLinkDelegate>(delegate);
}

bool ChromeManagementAPIDelegate::IsWebAppInstalled(
    content::BrowserContext* context,
    const GURL& web_app_url) const {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(
      Profile::FromBrowserContext(context));
  DCHECK(provider);
  return provider->registrar().IsInstalled(web_app_url);
}

bool ChromeManagementAPIDelegate::CanContextInstallWebApps(
    content::BrowserContext* context) const {
  return web_app::AreWebAppsUserInstallable(
      Profile::FromBrowserContext(context));
}

void ChromeManagementAPIDelegate::InstallReplacementWebApp(
    content::BrowserContext* context,
    const GURL& web_app_url,
    InstallWebAppCallback callback) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile);
  DCHECK(provider);

  provider->install_manager().LoadWebAppAndCheckInstallability(
      web_app_url, base::BindOnce(&OnDidInstallWebAppInstallableCheck, profile,
                                  std::move(callback)));
}

void ChromeManagementAPIDelegate::EnableExtension(
    content::BrowserContext* context,
    const std::string& extension_id) const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  // If the extension was disabled for a permissions increase, the Management
  // API will have displayed a re-enable prompt to the user, so we know it's
  // safe to grant permissions here.
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->GrantPermissionsAndEnableExtension(extension);
}

void ChromeManagementAPIDelegate::DisableExtension(
    content::BrowserContext* context,
    const extensions::Extension* source_extension,
    const std::string& extension_id,
    extensions::disable_reason::DisableReason disable_reason) const {
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->DisableExtensionWithSource(source_extension, extension_id,
                                   disable_reason);
}

bool ChromeManagementAPIDelegate::UninstallExtension(
    content::BrowserContext* context,
    const std::string& transient_extension_id,
    extensions::UninstallReason reason,
    base::string16* error) const {
  return extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->UninstallExtension(transient_extension_id, reason, error);
}

void ChromeManagementAPIDelegate::SetLaunchType(
    content::BrowserContext* context,
    const std::string& extension_id,
    extensions::LaunchType launch_type) const {
  extensions::SetLaunchType(context, extension_id, launch_type);
}

GURL ChromeManagementAPIDelegate::GetIconURL(
    const extensions::Extension* extension,
    int icon_size,
    ExtensionIconSet::MatchType match,
    bool grayscale) const {
  return extensions::ExtensionIconSource::GetIconURL(extension, icon_size,
                                                     match, grayscale);
}
