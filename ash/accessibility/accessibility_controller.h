// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/accessibility/ax_enums.mojom.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ash {

class AccessibilityHighlightController;
class AccessibilityObserver;
class ScopedBacklightsForcedOff;
class SelectToSpeakEventHandler;
class SwitchAccessEventHandler;

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityController
    : public mojom::AccessibilityController,
      public SessionObserver,
      public TabletModeObserver {
 public:
  AccessibilityController();
  ~AccessibilityController() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  void AddObserver(AccessibilityObserver* observer);
  void RemoveObserver(AccessibilityObserver* observer);

  // Binds the mojom::AccessibilityController interface to this object.
  void BindRequest(mojom::AccessibilityControllerRequest request);

  // The following functions read and write to their associated preference.
  // These values are then used to determine whether the accelerator
  // confirmation dialog for the respective preference has been accepted before.
  void SetHighContrastAcceleratorDialogAccepted();
  bool HasHighContrastAcceleratorDialogBeenAccepted() const;
  void SetScreenMagnifierAcceleratorDialogAccepted();
  bool HasScreenMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDockedMagnifierAcceleratorDialogAccepted();
  bool HasDockedMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDictationAcceleratorDialogAccepted();
  bool HasDictationAcceleratorDialogBeenAccepted() const;
  void SetDisplayRotationAcceleratorDialogBeenAccepted();
  bool HasDisplayRotationAcceleratorDialogBeenAccepted() const;

  void SetAutoclickEnabled(bool enabled);
  bool autoclick_enabled() const { return autoclick_enabled_; }
  void SetAutoclickEventType(mojom::AutoclickEventType event_type);
  mojom::AutoclickEventType GetAutoclickEventType();
  void SetAutoclickMenuPosition(mojom::AutoclickMenuPosition position);
  mojom::AutoclickMenuPosition GetAutoclickMenuPosition();

  // Update the autoclick menu bounds if necessary. This may need to happen when
  // the display work area changes, or if system ui regions change (like the
  // virtual keyboard position).
  void UpdateAutoclickMenuBoundsIfNeeded();

  void SetCaretHighlightEnabled(bool enabled);
  bool caret_highlight_enabled() const { return caret_highlight_enabled_; }

  void SetCursorHighlightEnabled(bool enabled);
  bool cursor_highlight_enabled() const { return cursor_highlight_enabled_; }

  void SetDictationEnabled(bool enabled);
  bool dictation_enabled() const { return dictation_enabled_; }

  void SetFocusHighlightEnabled(bool enabled);
  bool focus_highlight_enabled() const { return focus_highlight_enabled_; }

  void SetFullscreenMagnifierEnabled(bool enabled);

  void SetHighContrastEnabled(bool enabled);
  bool high_contrast_enabled() const { return high_contrast_enabled_; }

  void SetLargeCursorEnabled(bool enabled);
  bool large_cursor_enabled() const { return large_cursor_enabled_; }

  void SetMonoAudioEnabled(bool enabled);
  bool mono_audio_enabled() const { return mono_audio_enabled_; }

  void SetSpokenFeedbackEnabled(bool enabled,
                                AccessibilityNotificationVisibility notify);
  bool spoken_feedback_enabled() const { return spoken_feedback_enabled_; }

  void SetSelectToSpeakEnabled(bool enabled);
  bool select_to_speak_enabled() const { return select_to_speak_enabled_; }

  void RequestSelectToSpeakStateChange();
  mojom::SelectToSpeakState GetSelectToSpeakState() const;

  void SetStickyKeysEnabled(bool enabled);
  bool sticky_keys_enabled() const { return sticky_keys_enabled_; }

  void SetSwitchAccessEnabled(bool enabled);
  bool switch_access_enabled() const { return switch_access_enabled_; }

  void SetSwitchAccessIgnoreVirtualKeyEvent(bool should_ignore);

  void SetVirtualKeyboardEnabled(bool enabled);
  bool virtual_keyboard_enabled() const { return virtual_keyboard_enabled_; }

  void SetDictationActive(bool is_active);
  bool dictation_active() const { return dictation_active_; }

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(mojom::AccessibilityAlert alert);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  void PlayEarcon(int32_t sound_key);

  // Initiates play of shutdown sound. The base::TimeDelta parameter gets the
  // shutdown duration.
  void PlayShutdownSound(base::OnceCallback<void(base::TimeDelta)> callback);

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture);

  // Toggle dictation.
  void ToggleDictation();

  // Cancels all current and queued speech immediately.
  void SilenceSpokenFeedback();

  // Called when we first detect two fingers are held down, which can be used to
  // toggle spoken feedback on some touch-only devices.
  void OnTwoFingerTouchStart();

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  void OnTwoFingerTouchStop();

  // Whether or not to enable toggling spoken feedback via holding down two
  // fingers on the screen.
  void ShouldToggleSpokenFeedbackViaTouch(
      base::OnceCallback<void(bool)> callback);

  // Plays tick sound indicating spoken feedback will be toggled after
  // countdown.
  void PlaySpokenFeedbackToggleCountdown(int tick_count);

  // Public because a11y features like screen magnifier are managed outside of
  // this controller.
  void NotifyAccessibilityStatusChanged();

  // mojom::AccessibilityController:
  void SetClient(mojom::AccessibilityControllerClientPtr client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override;
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override;
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override;
  void SetAccessibilityPanelBounds(
      const gfx::Rect& bounds,
      mojom::AccessibilityPanelState state) override;
  void SetSelectToSpeakState(mojom::SelectToSpeakState state) override;
  void SetSelectToSpeakEventHandlerDelegate(
      mojom::SelectToSpeakEventHandlerDelegatePtr delegate) override;
  void SetSwitchAccessKeysToCapture(
      const std::vector<int>& keys_to_capture) override;
  void SetSwitchAccessEventHandlerDelegate(
      mojom::SwitchAccessEventHandlerDelegatePtr delegate) override;
  void ToggleDictationFromSource(mojom::DictationToggleSource source) override;
  void ForwardKeyEventsToSwitchAccess(bool should_forward) override;
  void GetBatteryDescription(GetBatteryDescriptionCallback callback) override;
  void SetVirtualKeyboardVisible(bool is_visible) override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Test helpers:
  void FlushMojoForTest();
  SwitchAccessEventHandler* GetSwitchAccessEventHandlerForTest();

 private:
  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  void UpdateAutoclickFromPref();
  void UpdateAutoclickDelayFromPref();
  void UpdateAutoclickEventTypeFromPref();
  void UpdateAutoclickRevertToLeftClickFromPref();
  void UpdateAutoclickStabilizePositionFromPref();
  void UpdateAutoclickMovementThresholdFromPref();
  void UpdateAutoclickMenuPositionFromPref();
  void UpdateCaretHighlightFromPref();
  void UpdateCursorHighlightFromPref();
  void UpdateDictationFromPref();
  void UpdateFocusHighlightFromPref();
  void UpdateHighContrastFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateMonoAudioFromPref();
  void UpdateSpokenFeedbackFromPref();
  void UpdateSelectToSpeakFromPref();
  void UpdateStickyKeysFromPref();
  void UpdateSwitchAccessFromPref();
  void UpdateSwitchAccessKeyCodesFromPref(mojom::SwitchAccessCommand command);
  void UpdateVirtualKeyboardFromPref();
  void UpdateAccessibilityHighlightingFromPrefs();

  void MaybeCreateSelectToSpeakEventHandler();
  void MaybeCreateSwitchAccessEventHandler();

  // The pref service of the currently active user or the signin profile before
  // user logs in. Can be null in ash_unittests.
  PrefService* active_user_prefs_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Binding for mojom::AccessibilityController interface.
  mojo::BindingSet<mojom::AccessibilityController> bindings_;

  // Client interface in chrome browser.
  mojom::AccessibilityControllerClientPtr client_;

  bool autoclick_enabled_ = false;
  base::TimeDelta autoclick_delay_;
  bool caret_highlight_enabled_ = false;
  bool cursor_highlight_enabled_ = false;
  bool dictation_enabled_ = false;
  bool focus_highlight_enabled_ = false;
  bool high_contrast_enabled_ = false;
  bool large_cursor_enabled_ = false;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;
  bool mono_audio_enabled_ = false;
  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool sticky_keys_enabled_ = false;
  bool switch_access_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool dictation_active_ = false;

  mojom::SelectToSpeakState select_to_speak_state_ =
      mojom::SelectToSpeakState::kSelectToSpeakStateInactive;
  std::unique_ptr<SelectToSpeakEventHandler> select_to_speak_event_handler_;
  mojom::SelectToSpeakEventHandlerDelegatePtr
      select_to_speak_event_handler_delegate_ptr_;

  // List of key codes that Switch Access should capture.
  std::vector<int> switch_access_keys_to_capture_;
  std::unique_ptr<SwitchAccessEventHandler> switch_access_event_handler_;
  mojom::SwitchAccessEventHandlerDelegatePtr
      switch_access_event_handler_delegate_ptr_;

  // Used to control the highlights of caret, cursor and focus.
  std::unique_ptr<AccessibilityHighlightController>
      accessibility_highlight_controller_;

  // Used to force the backlights off to darken the screen.
  std::unique_ptr<ScopedBacklightsForcedOff> scoped_backlights_forced_off_;

  base::ObserverList<AccessibilityObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
