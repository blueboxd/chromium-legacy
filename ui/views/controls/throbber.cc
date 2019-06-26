// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace views {

// The default diameter of a Throbber. If you change this, also change
// kCheckmarkDipSize.
static constexpr int kDefaultDiameter = 16;
// The size of the checkmark, in DIP. This magic number matches the default
// diamater plus padding inherent in the checkmark SVG.
static constexpr int kCheckmarkDipSize = 18;

Throbber::Throbber() = default;

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (IsRunning())
    return;

  start_time_ = base::TimeTicks::Now();
  constexpr int kFrameTimeMs = 30;
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameTimeMs),
      base::BindRepeating(&Throbber::SchedulePaint, base::Unretained(this)));
  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!IsRunning())
    return;

  timer_.Stop();
  SchedulePaint();
}

bool Throbber::GetChecked() const {
  return checked_;
}

void Throbber::SetChecked(bool checked) {
  if (checked == checked_)
    return;

  checked_ = checked;
  OnPropertyChanged(&checked_, kPropertyEffectsPaint);
}

gfx::Size Throbber::CalculatePreferredSize() const {
  return gfx::Size(kDefaultDiameter, kDefaultDiameter);
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ThrobberSpinningColor);

  if (!IsRunning()) {
    if (checked_) {
      canvas->Translate(gfx::Vector2d((width() - kCheckmarkDipSize) / 2,
                                      (height() - kCheckmarkDipSize) / 2));
      gfx::PaintVectorIcon(canvas, vector_icons::kCheckCircleIcon,
                           kCheckmarkDipSize, color);
    }
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color, elapsed_time);
}

bool Throbber::IsRunning() const {
  return timer_.IsRunning();
}

BEGIN_METADATA(Throbber)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Throbber, bool, Checked)
END_METADATA()

// Smoothed throbber ---------------------------------------------------------

// Delay after work starts before starting throbber, in milliseconds.
static constexpr int kStartDelay = 200;

// Delay after work stops before stopping, in milliseconds.
static constexpr int kStopDelay = 50;

SmoothedThrobber::SmoothedThrobber()
    : start_delay_ms_(kStartDelay), stop_delay_ms_(kStopDelay) {
}

SmoothedThrobber::~SmoothedThrobber() = default;

void SmoothedThrobber::Start() {
  stop_timer_.Stop();

  if (!IsRunning() && !start_timer_.IsRunning()) {
    start_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(start_delay_ms_), this,
                       &SmoothedThrobber::StartDelayOver);
  }
}

void SmoothedThrobber::StartDelayOver() {
  Throbber::Start();
}

void SmoothedThrobber::Stop() {
  if (!IsRunning())
    start_timer_.Stop();

  stop_timer_.Stop();
  stop_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(stop_delay_ms_), this,
                    &SmoothedThrobber::StopDelayOver);
}

int SmoothedThrobber::GetStartDelayMs() const {
  return start_delay_ms_;
}

void SmoothedThrobber::SetStartDelayMs(int start_delay_ms) {
  if (start_delay_ms == start_delay_ms_)
    return;
  start_delay_ms_ = start_delay_ms;
  OnPropertyChanged(&start_delay_ms_, kPropertyEffectsNone);
}

int SmoothedThrobber::GetStopDelayMs() const {
  return stop_delay_ms_;
}

void SmoothedThrobber::SetStopDelayMs(int stop_delay_ms) {
  if (stop_delay_ms == stop_delay_ms_)
    return;
  stop_delay_ms_ = stop_delay_ms;
  OnPropertyChanged(&stop_delay_ms_, kPropertyEffectsNone);
}

void SmoothedThrobber::StopDelayOver() {
  Throbber::Stop();
}

BEGIN_METADATA(SmoothedThrobber)
METADATA_PARENT_CLASS(Throbber)
ADD_PROPERTY_METADATA(SmoothedThrobber, int, StartDelayMs)
ADD_PROPERTY_METADATA(SmoothedThrobber, int, StopDelayMs)
END_METADATA()

}  // namespace views
