// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// The time delta between clicks in which clicks to launch V2 apps are ignored.
const int kClickSuppressionInMS = 1000;

// AppMatcher is used to determine if various WebContents instances are
// associated with a specific app. Clients should call CanMatchWebContents()
// before iterating through WebContents instances and calling
// WebContentMatchesApp().
class AppMatcher {
 public:
  AppMatcher(Profile* profile,
             const std::string& app_id,
             const URLPattern& refocus_pattern)
      : app_id_(app_id), refocus_pattern_(refocus_pattern) {
    DCHECK(profile);
    if (web_app::WebAppProviderBase* provider =
            web_app::WebAppProviderBase::GetProviderBase(profile)) {
      if (provider->registrar().IsLocallyInstalled(app_id)) {
        registrar_ = &provider->registrar();
      }
    }
    if (!registrar_)
      extension_ = GetExtensionForAppID(app_id, profile);
  }

  AppMatcher(const AppMatcher&) = delete;
  AppMatcher& operator=(const AppMatcher&) = delete;

  bool CanMatchWebContents() const { return registrar_ || extension_; }

  // Returns true if this app matches the given |web_contents|. If
  // |deprecated_is_app| is true, the application gets first checked against its
  // original URL since a windowed app might have navigated away from its app
  // domain.
  // May only be called if CanMatchWebContents() return true.
  bool WebContentMatchesApp(content::WebContents* web_contents,
                            Browser* browser) const {
    DCHECK(CanMatchWebContents());
    return extension_ ? WebContentMatchesHostedApp(web_contents, browser)
                      : WebContentMatchesWebApp(web_contents, browser);
  }

 private:
  bool WebContentMatchesHostedApp(content::WebContents* web_contents,
                                  Browser* browser) const {
    DCHECK(extension_);
    DCHECK(!registrar_);

    // If the browser is an app window, and the app name matches the extension,
    // then the contents match the app.
    if (browser->deprecated_is_app()) {
      const Extension* browser_extension =
          ExtensionRegistry::Get(browser->profile())
              ->GetExtensionById(
                  web_app::GetAppIdFromApplicationName(browser->app_name()),
                  ExtensionRegistry::EVERYTHING);
      return browser_extension == extension_;
    }

    // Apps set to launch in app windows should not match contents running in
    // tabs.
    if (extensions::LaunchesInWindow(browser->profile(), extension_))
      return false;

    // There are three ways to identify the association of a URL with this
    // extension:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The extension's origin + extent gets matched.
    // - The launcher controller knows that the tab got created for this app.
    const GURL tab_url = web_contents->GetURL();
    return (
        (!refocus_pattern_.match_all_urls() &&
         refocus_pattern_.MatchesURL(tab_url)) ||
        (extension_->OverlapsWithOrigin(tab_url) &&
         extension_->web_extent().MatchesURL(tab_url)) ||
        ChromeLauncherController::instance()->IsWebContentHandledByApplication(
            web_contents, app_id_));
  }

  // Returns true if this web app matches the given |web_contents|. If
  // |deprecated_is_app| is true, the application gets first checked against its
  // original URL since a windowed app might have navigated away from its app
  // domain.
  bool WebContentMatchesWebApp(content::WebContents* web_contents,
                               Browser* browser) const {
    DCHECK(registrar_);
    DCHECK(!extension_);

    // If the browser is a web app window, and the window app id matches,
    // then the contents match the app.
    if (browser->app_controller() && browser->app_controller()->HasAppId())
      return browser->app_controller()->GetAppId() == app_id_;

    // Bookmark apps set to launch in app windows should not match contents
    // running in tabs.
    if (registrar_->GetAppUserDisplayMode(app_id_) ==
            web_app::DisplayMode::kStandalone &&
        !base::FeatureList::IsEnabled(
            features::kDesktopPWAsWithoutExtensions)) {
      return false;
    }

    // There are three ways to identify the association of a URL with this
    // web app:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The web app's scope gets matched.
    // - The launcher controller knows that the tab got created for this web
    // app.
    const GURL tab_url = web_contents->GetURL();
    base::Optional<GURL> app_scope = registrar_->GetAppScope(app_id_);
    DCHECK(app_scope.has_value());

    return (
        (!refocus_pattern_.match_all_urls() &&
         refocus_pattern_.MatchesURL(tab_url)) ||
        (base::StartsWith(tab_url.spec(), app_scope->spec(),
                          base::CompareCase::SENSITIVE)) ||
        ChromeLauncherController::instance()->IsWebContentHandledByApplication(
            web_contents, app_id_));
  }

  const std::string app_id_;
  const URLPattern refocus_pattern_;

  // AppMatcher is stack allocated. Pointer members below are not owned.

  // registrar_ is set when app_id_ is a web app.
  const web_app::AppRegistrar* registrar_ = nullptr;

  // extension_ is set when app_id_ is a hosted app.
  const Extension* extension_ = nullptr;
};

}  // namespace

// static
std::unique_ptr<AppShortcutLauncherItemController>
AppShortcutLauncherItemController::Create(const ash::ShelfID& shelf_id) {
  if (shelf_id.app_id == arc::kPlayStoreAppId)
    return std::make_unique<ArcPlaystoreShortcutLauncherItemController>();
  return base::WrapUnique<AppShortcutLauncherItemController>(
      new AppShortcutLauncherItemController(shelf_id));
}

// static
std::vector<content::WebContents*>
AppShortcutLauncherItemController::GetRunningApplications(
    const std::string& app_id,
    const GURL& refocus_url) {
  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url.spec());
  }

  Profile* const profile = ChromeLauncherController::instance()->profile();
  AppMatcher matcher(profile, app_id, refocus_pattern);

  std::vector<content::WebContents*> items;
  // It is possible to come here while an app gets loaded.
  if (!matcher.CanMatchWebContents())
    return items;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (matcher.WebContentMatchesApp(web_contents, browser))
        items.push_back(web_contents);
    }
  }
  return items;
}

AppShortcutLauncherItemController::AppShortcutLauncherItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {
  // To detect V1 applications we use their domain and match them against the
  // used URL. This will also work with applications like Google Drive.
  const Extension* extension = GetExtensionForAppID(
      shelf_id.app_id, ChromeLauncherController::instance()->profile());
  // Some unit tests have no real extension.
  if (extension) {
    set_refocus_url(GURL(
        extensions::AppLaunchInfo::GetLaunchWebURL(extension).spec() + "*"));
  }
}

AppShortcutLauncherItemController::~AppShortcutLauncherItemController() {}

void AppShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event && event->type() == ui::ET_KEY_RELEASED && AdvanceToNextApp()) {
    std::move(callback).Run(ash::SHELF_ACTION_WINDOW_ACTIVATED, {});
    return;
  }

  content::WebContents* content = GetLRUApplication();
  if (!content) {
    // Ideally we come here only once. After that ShellLauncherItemController
    // will take over when the shell window gets opened. However there are apps
    // which take a lot of time for pre-processing (like the files app) before
    // they open a window. Since there is currently no other way to detect if an
    // app was started we suppress any further clicks within a special time out.
    if (IsV2App() && !AllowNextLaunchAttempt()) {
      std::move(callback).Run(
          ash::SHELF_ACTION_NONE,
          GetAppMenuItems(event ? event->flags() : ui::EF_NONE));
      return;
    }

    // LaunchApp may replace and destroy this item controller instance. Run the
    // callback first and copy the id to avoid crashes.
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    ChromeLauncherController::instance()->LaunchApp(
        ash::ShelfID(shelf_id()), source, ui::EF_NONE, display_id);
    return;
  }

  if (GetRunningApplications().size() == 1) {
    std::move(callback).Run(ActivateContent(content), {});
  } else {
    // Multiple items, a menu will be shown. No need to activate the most
    // recently active item.
    std::move(callback).Run(
        ash::SHELF_ACTION_NONE,
        GetAppMenuItems(event ? event->flags() : ui::EF_NONE));
  }
}

ash::ShelfItemDelegate::AppMenuItems
AppShortcutLauncherItemController::GetAppMenuItems(int event_flags) {
  app_menu_items_ = GetRunningApplications();
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  AppMenuItems items;
  for (content::WebContents* item : app_menu_items_) {
    items.push_back({controller->GetAppMenuTitle(item),
                     controller->GetAppMenuIcon(item).AsImageSkia()});
  }
  return items;
}

void AppShortcutLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppShortcutLauncherItemController::ExecuteCommand(bool from_context_menu,
                                                       int64_t command_id,
                                                       int32_t event_flags,
                                                       int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  if (static_cast<size_t>(command_id) >= app_menu_items_.size()) {
    app_menu_items_.clear();
    return;
  }

  // If the web contents was destroyed while the menu was open, then the invalid
  // pointer cached in |app_menu_items_| should yield a null browser or kNoTab.
  content::WebContents* web_contents = app_menu_items_[command_id];
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  TabStripModel* tab_strip = browser ? browser->tab_strip_model() : nullptr;
  const int index = tab_strip ? tab_strip->GetIndexOfWebContents(web_contents)
                              : TabStripModel::kNoTab;
  if (index != TabStripModel::kNoTab) {
    if (event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON)) {
      tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_USER_GESTURE);
    } else {
      multi_user_util::MoveWindowToCurrentDesktop(
          browser->window()->GetNativeWindow());
      tab_strip->ActivateTabAt(index);
      browser->window()->Show();
      browser->window()->Activate();
    }
  }

  app_menu_items_.clear();
}

void AppShortcutLauncherItemController::Close() {
  // Close all running 'programs' of this type.
  std::vector<content::WebContents*> content =
      ChromeLauncherController::instance()->GetV1ApplicationsFromAppId(
          app_id());
  for (size_t i = 0; i < content.size(); i++) {
    Browser* browser = chrome::FindBrowserWithWebContents(content[i]);
    if (!browser ||
        !multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(content[i]);
    DCHECK(index != TabStripModel::kNoTab);
    tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_NONE);
  }
}

std::vector<content::WebContents*>
AppShortcutLauncherItemController::GetRunningApplications() {
  return GetRunningApplications(app_id(), refocus_url_);
}

content::WebContents* AppShortcutLauncherItemController::GetLRUApplication() {
  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url_.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url_.spec());
  }

  Profile* const profile = ChromeLauncherController::instance()->profile();
  AppMatcher matcher(profile, app_id(), refocus_pattern);

  // It is possible to come here while an app gets loaded.
  if (!matcher.CanMatchWebContents())
    return nullptr;

  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    // We start to enumerate from the active index.
    int active_index = tab_strip->active_index();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(
          (index + active_index) % tab_strip->count());
      if (matcher.WebContentMatchesApp(web_contents, browser))
        return web_contents;
    }
  }
  // Coming here our application was not in the LRU list. This could have
  // happened because it did never get activated yet. So check the browser list
  // as well.
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    Browser* browser = *it;
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (matcher.WebContentMatchesApp(web_contents, browser))
        return web_contents;
    }
  }
  return nullptr;
}

ash::ShelfAction AppShortcutLauncherItemController::ActivateContent(
    content::WebContents* content) {
  Browser* browser = chrome::FindBrowserWithWebContents(content);
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(content);
  DCHECK_NE(TabStripModel::kNoTab, index);

  int old_index = tab_strip->active_index();
  if (index != old_index)
    tab_strip->ActivateTabAt(index);
  return ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
      browser->window(),
      index == old_index && GetRunningApplications().size() == 1);
}

bool AppShortcutLauncherItemController::AdvanceToNextApp() {
  std::vector<content::WebContents*> items = GetRunningApplications();
  if (items.size() >= 1) {
    Browser* browser = chrome::FindLastActive();
    // The last active browser is not necessarily the active window. The window
    // could be a v2 app or ARC app.
    if (browser && browser->window()->IsActive()) {
      TabStripModel* tab_strip = browser->tab_strip_model();
      content::WebContents* active =
          tab_strip->GetWebContentsAt(tab_strip->active_index());
      std::vector<content::WebContents*>::const_iterator i(
          std::find(items.begin(), items.end(), active));
      if (i != items.end()) {
        if (items.size() == 1) {
          // If there is only a single item available, we animate it upon key
          // action.
          ash_util::BounceWindow(browser->window()->GetNativeWindow());
        } else {
          int index = (static_cast<int>(i - items.begin()) + 1) % items.size();
          ActivateContent(items[index]);
        }
        return true;
      }
    }
  }
  return false;
}

bool AppShortcutLauncherItemController::IsV2App() {
  const Extension* extension = GetExtensionForAppID(
      app_id(), ChromeLauncherController::instance()->profile());
  return extension && extension->is_platform_app();
}

bool AppShortcutLauncherItemController::AllowNextLaunchAttempt() {
  if (last_launch_attempt_.is_null() ||
      last_launch_attempt_ +
              base::TimeDelta::FromMilliseconds(kClickSuppressionInMS) <
          base::Time::Now()) {
    last_launch_attempt_ = base::Time::Now();
    return true;
  }
  return false;
}
