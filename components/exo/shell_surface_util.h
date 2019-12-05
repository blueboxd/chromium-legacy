// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
#define COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_

#include <memory>
#include <string>

#include "base/optional.h"

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace ui {
class LocatedEvent;
}

namespace exo {

class Permission;
class Surface;
class ShellSurfaceBase;

// Sets the application ID for the window. The application ID identifies the
// general class of applications to which the window belongs.
void SetShellApplicationId(aura::Window* window,
                           const base::Optional<std::string>& id);
const std::string* GetShellApplicationId(const aura::Window* window);

// Sets the startup ID for the window. The startup ID identifies the
// application using startup notification protocol.
void SetShellStartupId(aura::Window* window,
                       const base::Optional<std::string>& id);
const std::string* GetShellStartupId(aura::Window* window);

// Sets the main surface for the window.
void SetShellMainSurface(aura::Window* window, Surface* surface);

// Returns the main Surface instance or nullptr if it is not set.
// |window| must not be nullptr.
Surface* GetShellMainSurface(const aura::Window* window);

// Returns the ShellSurfaceBase for the given |window|, or nullptr if no such
// surface exists.
ShellSurfaceBase* GetShellSurfaceBaseForWindow(aura::Window* window);

// Returns the target surface for the located event |event|.  If an
// event handling is grabbed by an window, it'll first examine that
// window, then traverse to its transient parent if the parent also
// requested grab.
Surface* GetTargetSurfaceForLocatedEvent(ui::LocatedEvent* event);

// Allow the |window| to activate itself for the diration of |timeout|. Returns
// the permission object, where deleting the object ammounts to Revoke()ing the
// permission.
std::unique_ptr<exo::Permission> GrantPermissionToActivate(
    aura::Window* window,
    base::TimeDelta timeout);

// Returns true if the |window| has permission to activate itself.
bool HasPermissionToActivate(aura::Window* window);

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
