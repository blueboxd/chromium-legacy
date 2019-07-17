// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_

#include <string>
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"
#include "ui/accessibility/ax_enums.mojom.h"

// API function that enables or disables web content accessibility support.
class AccessibilityPrivateSetNativeAccessibilityEnabledFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetNativeAccessibilityEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeAccessibilityEnabled",
      ACCESSIBILITY_PRIVATE_SETNATIVEACCESSIBILITYENABLED)
};

// API function that sets the location of the accessibility focus ring.
class AccessibilityPrivateSetFocusRingsFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetFocusRingsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setFocusRings",
                             ACCESSIBILITY_PRIVATE_SETFOCUSRING)
};

// API function that sets the location of the accessibility highlights.
class AccessibilityPrivateSetHighlightsFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetHighlightsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setHighlights",
                             ACCESSIBILITY_PRIVATE_SETHIGHLIGHTS)
};

// API function that sets keyboard capture mode.
class AccessibilityPrivateSetKeyboardListenerFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetKeyboardListenerFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setKeyboardListener",
                             ACCESSIBILITY_PRIVATE_SETKEYBOARDLISTENER)
};

// API function that darkens or undarkens the screen.
class AccessibilityPrivateDarkenScreenFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateDarkenScreenFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.darkenScreen",
                             ACCESSIBILITY_PRIVATE_DARKENSCREEN)
};

// Opens a specified subpage in Chrome settings.
class AccessibilityPrivateOpenSettingsSubpageFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateOpenSettingsSubpageFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.openSettingsSubpage",
                             ACCESSIBILITY_PRIVATE_OPENSETTINGSSUBPAGE)
};

#if defined(OS_CHROMEOS)
// API function that sets the keys to be captured by Switch Access.
class AccessibilityPrivateSetSwitchAccessKeysFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetSwitchAccessKeysFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSwitchAccessKeys",
                             ACCESSIBILITY_PRIVATE_SETSWITCHACCESSKEYS)
};

// API function that sets native ChromeVox ARC support.
class AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction()
      override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp",
      ACCESSIBILITY_PRIVATE_SETNATIVECHROMEVOXARCSUPPORTFORCURRENTAPP)
};

// API function that injects key events.
class AccessibilityPrivateSendSyntheticKeyEventFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSendSyntheticKeyEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticKeyEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICKEYEVENT)
};

// API function that enables or disables mouse events in ChromeVox.
class AccessibilityPrivateEnableChromeVoxMouseEventsFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateEnableChromeVoxMouseEventsFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.enableChromeVoxMouseEvents",
                             ACCESSIBILITY_PRIVATE_ENABLECHROMEVOXMOUSEEVENTS)
};

// API function that injects mouse events.
class AccessibilityPrivateSendSyntheticMouseEventFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSendSyntheticMouseEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.sendSyntheticMouseEvent",
                             ACCESSIBILITY_PRIVATE_SENDSYNTHETICMOUSEEVENT)
};

// API function that is called when the Select-to-Speak extension state changes.
class AccessibilityPrivateOnSelectToSpeakStateChangedFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateOnSelectToSpeakStateChangedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.onSelectToSpeakStateChanged",
                             ACCESSIBILITY_PRIVATE_ONSELECTTOSPEAKSTATECHANGED)
};

// API function that is called when the Autoclick extension finds scrollable
// bounds.
class AccessibilityPrivateOnScrollableBoundsForPointFoundFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateOnScrollableBoundsForPointFoundFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.onScrollableBoundsForPointFound",
      ACCESSIBILITY_PRIVATE_ONSCROLLABLEBOUNDSFORPOINTFOUND)
};

// API function that is called when a user toggles Dictation from another
// acessibility feature.
class AccessibilityPrivateToggleDictationFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateToggleDictationFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.toggleDictation",
                             ACCESSIBILITY_PRIVATE_TOGGLEDICTATION)
};

// API function that is called to show or hide the Switch Access menu.
class AccessibilityPrivateSetSwitchAccessMenuStateFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetSwitchAccessMenuStateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setSwitchAccessMenuState",
                             ACCESSIBILITY_PRIVATE_SETSWITCHACCESSMENUSTATE)
};

// API function that requests that key events be forwarded to the Switch
// Access extension.
class AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.forwardKeyEventsToSwitchAccess",
      ACCESSIBILITY_PRIVATE_FORWARDKEYEVENTSTOSWITCHACCESS)
};

// API function that is called to get the device's battery status as a string.
class AccessibilityPrivateGetBatteryDescriptionFunction
    : public UIThreadExtensionFunction {
 public:
  AccessibilityPrivateGetBatteryDescriptionFunction();
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.getBatteryDescription",
                             ACCESSIBILITY_PRIVATE_GETBATTERYDESCRIPTION)

 private:
  ~AccessibilityPrivateGetBatteryDescriptionFunction() override;
};

// API function that opens or closes the virtual keyboard.
class AccessibilityPrivateSetVirtualKeyboardVisibleFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetVirtualKeyboardVisibleFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setVirtualKeyboardVisible",
                             ACCESSIBILITY_PRIVATE_SETVIRTUALKEYBOARDVISIBLE)
};
#endif  // defined (OS_CHROMEOS)

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
