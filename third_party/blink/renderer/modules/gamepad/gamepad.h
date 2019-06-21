/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_H_

#include "device/gamepad/public/cpp/gamepad.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_button.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_haptic_actuator.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_pose.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MODULES_EXPORT Gamepad final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Objects implementing this interface are garbage-collected.
  class Client : public GarbageCollectedMixin {
   public:
    virtual GamepadHapticActuator* GetVibrationActuatorForGamepad(
        const Gamepad&) = 0;
    virtual ~Client() = default;
  };

  Gamepad(Client* client,
          unsigned index,
          base::TimeTicks time_origin,
          base::TimeTicks time_floor);
  ~Gamepad() override;

  void UpdateFromDeviceState(const device::Gamepad&);

  typedef Vector<double> DoubleVector;

  const String& id() const { return id_; }
  void SetId(const String& id) { id_ = id; }

  unsigned index() const { return index_; }

  bool connected() const { return connected_; }
  void SetConnected(bool val) { connected_ = val; }

  DOMHighResTimeStamp timestamp() const { return timestamp_; }

  const String& mapping() const { return mapping_; }
  void SetMapping(device::GamepadMapping mapping);

  const DoubleVector& axes();
  void SetAxes(unsigned count, const double* data);
  bool isAxisDataDirty() const { return is_axis_data_dirty_; }

  const GamepadButtonVector& buttons();
  void SetButtons(unsigned count, const device::GamepadButton* data);
  bool isButtonDataDirty() const { return is_button_data_dirty_; }

  GamepadHapticActuator* vibrationActuator() const;
  void SetVibrationActuatorInfo(const device::GamepadHapticActuator&);
  bool HasVibrationActuator() const { return has_vibration_actuator_; }
  device::GamepadHapticActuatorType GetVibrationActuatorType() const {
    return vibration_actuator_type_;
  }

  GamepadPose* pose() const { return pose_; }
  void SetPose(const device::GamepadPose&);

  const String& hand() const { return hand_; }
  void SetHand(const device::GamepadHand&);

  unsigned displayId() const { return display_id_; }
  void SetDisplayId(unsigned val) { display_id_ = val; }

  void Trace(blink::Visitor*) override;

 private:
  void SetTimestamp(const device::Gamepad& device_gamepad);

  Member<Client> client_;

  // A string identifying the gamepad model.
  String id_;

  // The index of this gamepad within the GamepadList.
  const unsigned index_;

  // True if this gamepad was still connected when gamepad state was captured.
  bool connected_;

  // The current time when the gamepad state was captured.
  DOMHighResTimeStamp timestamp_;

  // A string indicating whether the standard mapping is in use.
  String mapping_;

  // Snapshot of the axis state.
  DoubleVector axes_;

  // Snapshot of the button state.
  GamepadButtonVector buttons_;

  // True if the gamepad can produce haptic vibration effects.
  bool has_vibration_actuator_;

  // The type of haptic actuator used for vibration effects.
  device::GamepadHapticActuatorType vibration_actuator_type_;

  // Snapshot of the gamepad pose.
  Member<GamepadPose> pose_;

  // A string representing the handedness of the gamepad.
  String hand_;

  // An identifier for associating a gamepad with a VR headset.
  unsigned display_id_;

  // True if the data in |axes_| has changed since the last time it was
  // accessed.
  bool is_axis_data_dirty_;

  // True if the data in |buttons_| has changed since the last time it was
  // accessed.
  bool is_button_data_dirty_;

  // Base time on which all relative timestamps are based.
  const base::TimeTicks time_origin_;

  // Minimum value to use for timestamps from the device.
  const base::TimeTicks time_floor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_H_
