// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_dock_tile.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"

using PermissionOverrides = std::set<content::PermissionType>;
using protocol::Maybe;
using protocol::Response;

namespace {

BrowserWindow* GetBrowserWindow(int window_id) {
  for (auto* b : *BrowserList::GetInstance()) {
    if (b->session_id().id() == window_id)
      return b->window();
  }
  return nullptr;
}

std::unique_ptr<protocol::Browser::Bounds> GetBrowserWindowBounds(
    BrowserWindow* window) {
  std::string window_state = "normal";
  if (window->IsMinimized())
    window_state = "minimized";
  if (window->IsMaximized())
    window_state = "maximized";
  if (window->IsFullscreen())
    window_state = "fullscreen";

  gfx::Rect bounds;
  if (window->IsMinimized())
    bounds = window->GetRestoredBounds();
  else
    bounds = window->GetBounds();
  return protocol::Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(window_state)
      .Build();
}

Response FromProtocolPermissionType(
    const protocol::Browser::PermissionType& type,
    content::PermissionType* out_type) {
  if (type == protocol::Browser::PermissionTypeEnum::Notifications) {
    *out_type = content::PermissionType::NOTIFICATIONS;
  } else if (type == protocol::Browser::PermissionTypeEnum::Geolocation) {
    *out_type = content::PermissionType::GEOLOCATION;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::ProtectedMediaIdentifier) {
    *out_type = content::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (type == protocol::Browser::PermissionTypeEnum::Midi) {
    *out_type = content::PermissionType::MIDI;
  } else if (type == protocol::Browser::PermissionTypeEnum::MidiSysex) {
    *out_type = content::PermissionType::MIDI_SYSEX;
  } else if (type == protocol::Browser::PermissionTypeEnum::DurableStorage) {
    *out_type = content::PermissionType::DURABLE_STORAGE;
  } else if (type == protocol::Browser::PermissionTypeEnum::AudioCapture) {
    *out_type = content::PermissionType::AUDIO_CAPTURE;
  } else if (type == protocol::Browser::PermissionTypeEnum::VideoCapture) {
    *out_type = content::PermissionType::VIDEO_CAPTURE;
  } else if (type == protocol::Browser::PermissionTypeEnum::BackgroundSync) {
    *out_type = content::PermissionType::BACKGROUND_SYNC;
  } else if (type == protocol::Browser::PermissionTypeEnum::Flash) {
    *out_type = content::PermissionType::FLASH;
  } else if (type == protocol::Browser::PermissionTypeEnum::Sensors) {
    *out_type = content::PermissionType::SENSORS;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::AccessibilityEvents) {
    *out_type = content::PermissionType::ACCESSIBILITY_EVENTS;
  } else if (type == protocol::Browser::PermissionTypeEnum::ClipboardRead) {
    *out_type = content::PermissionType::CLIPBOARD_READ;
  } else if (type == protocol::Browser::PermissionTypeEnum::ClipboardWrite) {
    *out_type = content::PermissionType::CLIPBOARD_WRITE;
  } else if (type == protocol::Browser::PermissionTypeEnum::PaymentHandler) {
    *out_type = content::PermissionType::PAYMENT_HANDLER;
  } else if (type == protocol::Browser::PermissionTypeEnum::BackgroundFetch) {
    *out_type = content::PermissionType::BACKGROUND_FETCH;
  } else if (type == protocol::Browser::PermissionTypeEnum::WakeLockScreen) {
    *out_type = content::PermissionType::WAKE_LOCK_SCREEN;
  } else if (type == protocol::Browser::PermissionTypeEnum::WakeLockSystem) {
    *out_type = content::PermissionType::WAKE_LOCK_SYSTEM;
  } else {
    return Response::InvalidParams("Unknown permission type: " + type);
  }
  return Response::OK();
}

}  // namespace

BrowserHandler::BrowserHandler(protocol::UberDispatcher* dispatcher,
                               const std::string& target_id)
    : target_id_(target_id) {
  // Dispatcher can be null in tests.
  if (dispatcher)
    protocol::Browser::Dispatcher::wire(dispatcher, this);
}

BrowserHandler::~BrowserHandler() = default;

Response BrowserHandler::GetWindowForTarget(
    protocol::Maybe<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  auto host =
      content::DevToolsAgentHost::GetForId(target_id.fromMaybe(target_id_));
  if (!host)
    return Response::Error("No target with given id");
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents)
    return Response::Error("No web contents in the target");

  Browser* browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    int tab_index = b->tab_strip_model()->GetIndexOfWebContents(web_contents);
    if (tab_index != TabStripModel::kNoTab)
      browser = b;
  }
  if (!browser)
    return Response::Error("Browser window not found");

  BrowserWindow* window = browser->window();
  *out_window_id = browser->session_id().id();
  *out_bounds = GetBrowserWindowBounds(window);
  return Response::OK();
}

Response BrowserHandler::GetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return Response::Error("Browser window not found");

  *out_bounds = GetBrowserWindowBounds(window);
  return Response::OK();
}

Response BrowserHandler::Close() {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce([]() { chrome::ExitIgnoreUnloadHandlers(); }));
  return Response::OK();
}

Response BrowserHandler::SetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds> window_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return Response::Error("Browser window not found");
  gfx::Rect bounds = window->GetBounds();
  const bool set_bounds = window_bounds->HasLeft() || window_bounds->HasTop() ||
                          window_bounds->HasWidth() ||
                          window_bounds->HasHeight();
  if (set_bounds) {
    bounds.set_x(window_bounds->GetLeft(bounds.x()));
    bounds.set_y(window_bounds->GetTop(bounds.y()));
    bounds.set_width(window_bounds->GetWidth(bounds.width()));
    bounds.set_height(window_bounds->GetHeight(bounds.height()));
  }

  const std::string window_state = window_bounds->GetWindowState("normal");
  if (set_bounds && window_state != "normal") {
    return Response::Error(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (window_state == "fullscreen") {
    if (window->IsMinimized()) {
      return Response::Error(
          "To make minimized window fullscreen, "
          "restore it to normal state first.");
    }
    window->GetExclusiveAccessContext()->EnterFullscreen(
        GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
  } else if (window_state == "maximized") {
    if (window->IsMinimized() || window->IsFullscreen()) {
      return Response::Error(
          "To maximize a minimized or fullscreen "
          "window, restore it to normal state first.");
    }
    window->Maximize();
  } else if (window_state == "minimized") {
    if (window->IsFullscreen()) {
      return Response::Error(
          "To minimize a fullscreen window, restore it to normal "
          "state first.");
    }
    window->Minimize();
  } else if (window_state == "normal") {
    if (window->IsFullscreen())
      window->GetExclusiveAccessContext()->ExitFullscreen();
    else if (window->IsMinimized())
      window->Show();
    else if (window->IsMaximized())
      window->Restore();
    else if (set_bounds)
      window->SetBounds(bounds);
  } else {
    NOTREACHED();
  }

  return Response::OK();
}

Response BrowserHandler::Disable() {
  for (auto& browser_context_id : contexts_with_overridden_permissions_) {
    Profile* profile = nullptr;
    Maybe<std::string> context_id =
        browser_context_id.empty() ? Maybe<std::string>()
                                   : Maybe<std::string>(browser_context_id);
    FindProfile(context_id, &profile);
    if (profile) {
      PermissionManager* permission_manager = PermissionManager::Get(profile);
      permission_manager->ResetPermissionOverridesForDevTools();
    }
  }
  contexts_with_overridden_permissions_.clear();
  return Response::OK();
}

Response BrowserHandler::GrantPermissions(
    const std::string& origin,
    std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
        permissions,
    Maybe<std::string> browser_context_id) {
  Profile* profile = nullptr;
  Response response = FindProfile(browser_context_id, &profile);
  if (!response.isSuccess())
    return response;

  PermissionOverrides overrides;
  for (const std::string& permission : *permissions) {
    content::PermissionType type;
    Response type_response = FromProtocolPermissionType(permission, &type);
    if (!type_response.isSuccess())
      return type_response;
    overrides.insert(type);
  }

  PermissionManager* permission_manager = PermissionManager::Get(profile);
  GURL url = GURL(origin).GetOrigin();
  permission_manager->SetPermissionOverridesForDevTools(url,
                                                        std::move(overrides));
  contexts_with_overridden_permissions_.insert(
      browser_context_id.fromMaybe(""));
  return Response::FallThrough();
}

Response BrowserHandler::ResetPermissions(
    Maybe<std::string> browser_context_id) {
  Profile* profile = nullptr;
  Response response = FindProfile(browser_context_id, &profile);
  if (!response.isSuccess())
    return response;
  PermissionManager* permission_manager = PermissionManager::Get(profile);
  permission_manager->ResetPermissionOverridesForDevTools();
  contexts_with_overridden_permissions_.erase(browser_context_id.fromMaybe(""));
  return Response::FallThrough();
}

protocol::Response BrowserHandler::SetDockTile(
    protocol::Maybe<std::string> label,
    protocol::Maybe<protocol::Binary> image) {
  std::vector<gfx::ImagePNGRep> reps;
  if (image.isJust())
    reps.emplace_back(image.fromJust().bytes(), 1);
  DevToolsDockTile::Update(label.fromMaybe(std::string()),
                           !reps.empty() ? gfx::Image(reps) : gfx::Image());
  return Response::OK();
}

Response BrowserHandler::FindProfile(
    const Maybe<std::string>& browser_context_id,
    Profile** profile) {
  auto* delegate = ChromeDevToolsManagerDelegate::GetInstance();
  if (!browser_context_id.isJust()) {
    *profile =
        Profile::FromBrowserContext(delegate->GetDefaultBrowserContext());
    if (*profile == nullptr)
      return Response::Error("Browser context management is not supported.");
    return Response::OK();
  }

  std::string context_id = browser_context_id.fromJust();
  for (auto* context : delegate->GetBrowserContexts()) {
    if (context->UniqueId() == context_id) {
      *profile = Profile::FromBrowserContext(context);
      return Response::OK();
    }
  }
  return Response::InvalidParams("Failed to find browser context for id " +
                                 context_id);
}
