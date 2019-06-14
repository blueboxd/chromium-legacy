// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/mock_constraint_factory.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_processor_options.h"

namespace blink {

MockConstraintFactory::MockConstraintFactory() {}

MockConstraintFactory::~MockConstraintFactory() {}

WebMediaTrackConstraintSet& MockConstraintFactory::AddAdvanced() {
  advanced_.emplace_back();
  return advanced_.back();
}

WebMediaConstraints MockConstraintFactory::CreateWebMediaConstraints() const {
  WebMediaConstraints constraints;
  constraints.Initialize(basic_, advanced_);
  return constraints;
}

void MockConstraintFactory::DisableDefaultAudioConstraints() {
  basic_.goog_echo_cancellation.SetExact(false);
  basic_.goog_experimental_echo_cancellation.SetExact(false);
  basic_.goog_auto_gain_control.SetExact(false);
  basic_.goog_experimental_auto_gain_control.SetExact(false);
  basic_.goog_noise_suppression.SetExact(false);
  basic_.goog_noise_suppression.SetExact(false);
  basic_.goog_highpass_filter.SetExact(false);
  basic_.goog_experimental_noise_suppression.SetExact(false);
}

void MockConstraintFactory::DisableAecAudioConstraints() {
  basic_.goog_echo_cancellation.SetExact(false);
}

void MockConstraintFactory::Reset() {
  basic_ = WebMediaTrackConstraintSet();
  advanced_.clear();
}

}  // namespace blink
