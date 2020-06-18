/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
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

#include <algorithm>
#include <limits>

#include "build/build_config.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// Breakpoints where we deicde to do linear interoplation, 3-point
// interpolation or 5-point interpolation.  See DoInterpolation().
constexpr float kInterpolate2Point = 0.3;
constexpr float kInterpolate3Point = 0.16;

OscillatorHandler::OscillatorHandler(AudioNode& node,
                                     float sample_rate,
                                     const String& oscillator_type,
                                     PeriodicWave* wave_table,
                                     AudioParamHandler& frequency,
                                     AudioParamHandler& detune)
    : AudioScheduledSourceHandler(kNodeTypeOscillator, node, sample_rate),
      frequency_(&frequency),
      detune_(&detune),
      first_render_(true),
      virtual_read_index_(0),
      phase_increments_(audio_utilities::kRenderQuantumFrames),
      detune_values_(audio_utilities::kRenderQuantumFrames) {
  if (wave_table) {
    // A PeriodicWave overrides any value for the oscillator type,
    // forcing the type to be 'custom".
    SetPeriodicWave(wave_table);
  } else {
    if (oscillator_type == "sine")
      SetType(SINE);
    else if (oscillator_type == "square")
      SetType(SQUARE);
    else if (oscillator_type == "sawtooth")
      SetType(SAWTOOTH);
    else if (oscillator_type == "triangle")
      SetType(TRIANGLE);
    else
      NOTREACHED();
  }

  // An oscillator is always mono.
  AddOutput(1);

  Initialize();
}

scoped_refptr<OscillatorHandler> OscillatorHandler::Create(
    AudioNode& node,
    float sample_rate,
    const String& oscillator_type,
    PeriodicWave* wave_table,
    AudioParamHandler& frequency,
    AudioParamHandler& detune) {
  return base::AdoptRef(new OscillatorHandler(
      node, sample_rate, oscillator_type, wave_table, frequency, detune));
}

OscillatorHandler::~OscillatorHandler() {
  Uninitialize();
}

String OscillatorHandler::GetType() const {
  switch (type_) {
    case SINE:
      return "sine";
    case SQUARE:
      return "square";
    case SAWTOOTH:
      return "sawtooth";
    case TRIANGLE:
      return "triangle";
    case CUSTOM:
      return "custom";
    default:
      NOTREACHED();
      return "custom";
  }
}

void OscillatorHandler::SetType(const String& type,
                                ExceptionState& exception_state) {
  if (type == "sine") {
    SetType(SINE);
  } else if (type == "square") {
    SetType(SQUARE);
  } else if (type == "sawtooth") {
    SetType(SAWTOOTH);
  } else if (type == "triangle") {
    SetType(TRIANGLE);
  } else if (type == "custom") {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "'type' cannot be set directly to "
                                      "'custom'.  Use setPeriodicWave() to "
                                      "create a custom Oscillator type.");
  }
}

bool OscillatorHandler::SetType(uint8_t type) {
  PeriodicWave* periodic_wave = nullptr;

  switch (type) {
    case SINE:
      periodic_wave = Context()->GetPeriodicWave(SINE);
      break;
    case SQUARE:
      periodic_wave = Context()->GetPeriodicWave(SQUARE);
      break;
    case SAWTOOTH:
      periodic_wave = Context()->GetPeriodicWave(SAWTOOTH);
      break;
    case TRIANGLE:
      periodic_wave = Context()->GetPeriodicWave(TRIANGLE);
      break;
    case CUSTOM:
    default:
      // Return false for invalid types, including CUSTOM since
      // setPeriodicWave() method must be called explicitly.
      NOTREACHED();
      return false;
  }

  SetPeriodicWave(periodic_wave);
  type_ = type;
  return true;
}

// Convert the detune value (in cents) to a frequency scale multiplier:
// 2^(d/1200)
static float DetuneToFrequencyMultiplier(float detune_value) {
  return std::exp2(detune_value / 1200);
}

// Clamp the frequency value to lie with Nyquist frequency. For NaN, arbitrarily
// clamp to +Nyquist.
static void ClampFrequency(float* frequency,
                           int frames_to_process,
                           float nyquist) {
  for (int k = 0; k < frames_to_process; ++k) {
    float f = frequency[k];

    if (std::isnan(f)) {
      frequency[k] = nyquist;
    } else {
      frequency[k] = clampTo(f, -nyquist, nyquist);
    }
  }
}

bool OscillatorHandler::CalculateSampleAccuratePhaseIncrements(
    uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, phase_increments_.size());
  DCHECK_LE(frames_to_process, detune_values_.size());

  if (first_render_) {
    first_render_ = false;
    frequency_->ResetSmoothedValue();
    detune_->ResetSmoothedValue();
  }

  bool has_sample_accurate_values = false;
  bool has_frequency_changes = false;
  float* phase_increments = phase_increments_.Data();

  float final_scale = periodic_wave_->RateScale();

  if (frequency_->HasSampleAccurateValues() && frequency_->IsAudioRate()) {
    has_sample_accurate_values = true;
    has_frequency_changes = true;

    // Get the sample-accurate frequency values and convert to phase increments.
    // They will be converted to phase increments below.
    frequency_->CalculateSampleAccurateValues(phase_increments,
                                              frames_to_process);
  } else {
    // Handle ordinary parameter changes if there are no scheduled changes.
    float frequency = frequency_->FinalValue();
    final_scale *= frequency;
  }

  if (detune_->HasSampleAccurateValues() && detune_->IsAudioRate()) {
    has_sample_accurate_values = true;

    // Get the sample-accurate detune values.
    float* detune_values =
        has_frequency_changes ? detune_values_.Data() : phase_increments;
    detune_->CalculateSampleAccurateValues(detune_values, frames_to_process);

    // Convert from cents to rate scalar.
    float k = 1.0 / 1200;
    vector_math::Vsmul(detune_values, 1, &k, detune_values, 1,
                       frames_to_process);
    for (unsigned i = 0; i < frames_to_process; ++i) {
      detune_values[i] = std::exp2(detune_values[i]);
    }

    if (has_frequency_changes) {
      // Multiply frequencies by detune scalings.
      vector_math::Vmul(detune_values, 1, phase_increments, 1, phase_increments,
                        1, frames_to_process);
    }
  } else {
    // Handle ordinary parameter changes if there are no scheduled
    // changes.
    float detune = detune_->FinalValue();
    float detune_scale = DetuneToFrequencyMultiplier(detune);
    final_scale *= detune_scale;
  }

  if (has_sample_accurate_values) {
    ClampFrequency(phase_increments, frames_to_process,
                   Context()->sampleRate() / 2);
    // Convert from frequency to wavetable increment.
    vector_math::Vsmul(phase_increments, 1, &final_scale, phase_increments, 1,
                       frames_to_process);
  }

  return has_sample_accurate_values;
}

static float DoInterpolation(double virtual_read_index,
                             float incr,
                             unsigned read_index_mask,
                             float table_interpolation_factor,
                             const float* lower_wave_data,
                             const float* higher_wave_data) {
  DCHECK_GE(incr, 0);
  DCHECK(std::isfinite(virtual_read_index));

  double sample_lower = 0;
  double sample_higher = 0;

  unsigned read_index_0 = static_cast<unsigned>(virtual_read_index);

  // Consider a typical sample rate of 44100 Hz and max periodic wave
  // size of 4096.  The relationship between |incr| and the frequency
  // of the oscillator is |incr| = freq * 4096/44100. Or freq =
  // |incr|*44100/4096 = 10.8*|incr|.
  //
  // For the |incr| thresholds below, this means that we use linear
  // interpolation for all freq >= 3.2 Hz, 3-point Lagrange
  // for freq >= 1.7 Hz and 5-point Lagrange for every thing else.
  //
  // We use Lagrange interpolation because it's relatively simple to
  // implement and fairly inexpensive, and the interpolator always
  // passes through known points.
  if (incr >= kInterpolate2Point) {
    // Increment is fairly large, so we're doing no more than about 3
    // points between each wave table entry. Assume linear
    // interpolation between points is good enough.
    unsigned read_index2 = read_index_0 + 1;

    // Contain within valid range.
    read_index_0 = read_index_0 & read_index_mask;
    read_index2 = read_index2 & read_index_mask;

    float sample1_lower = lower_wave_data[read_index_0];
    float sample2_lower = lower_wave_data[read_index2];
    float sample1_higher = higher_wave_data[read_index_0];
    float sample2_higher = higher_wave_data[read_index2];

    // Linearly interpolate within each table (lower and higher).
    double interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index_0;
    sample_higher = (1 - interpolation_factor) * sample1_higher +
                    interpolation_factor * sample2_higher;
    sample_lower = (1 - interpolation_factor) * sample1_lower +
                   interpolation_factor * sample2_lower;

  } else if (incr >= kInterpolate3Point) {
    // We're doing about 6 interpolation values between each wave
    // table sample. Just use a 3-point Lagrange interpolator to get a
    // better estimate than just linear.
    //
    // See 3-point formula in http://dlmf.nist.gov/3.3#ii
    unsigned read_index[3];

    for (int k = -1; k <= 1; ++k) {
      read_index[k + 1] = (read_index_0 + k) & read_index_mask;
    }

    double a[3];
    double t = virtual_read_index - read_index_0;

    a[0] = 0.5 * t * (t - 1);
    a[1] = 1 - t * t;
    a[2] = 0.5 * t * (t + 1);

    for (int k = 0; k < 3; ++k) {
      sample_lower += a[k] * lower_wave_data[read_index[k]];
      sample_higher += a[k] * higher_wave_data[read_index[k]];
    }
  } else {
    // For everything else (more than 6 points per entry), we'll do a
    // 5-point Lagrange interpolator.  This is a trade-off between
    // quality and speed.
    //
    // See 5-point formula in http://dlmf.nist.gov/3.3#ii
    unsigned read_index[5];
    for (int k = -2; k <= 2; ++k) {
      read_index[k + 2] = (read_index_0 + k) & read_index_mask;
    }

    double a[5];
    double t = virtual_read_index - read_index_0;
    double t2 = t * t;

    a[0] = t * (t2 - 1) * (t - 2) / 24;
    a[1] = -t * (t - 1) * (t2 - 4) / 6;
    a[2] = (t2 - 1) * (t2 - 4) / 4;
    a[3] = -t * (t + 1) * (t2 - 4) / 6;
    a[4] = t * (t2 - 1) * (t + 2) / 24;

    for (int k = 0; k < 5; ++k) {
      sample_lower += a[k] * lower_wave_data[read_index[k]];
      sample_higher += a[k] * higher_wave_data[read_index[k]];
    }
  }

  // Then interpolate between the two tables.
  float sample = (1 - table_interpolation_factor) * sample_higher +
                 table_interpolation_factor * sample_lower;
  return sample;
}

#if defined(ARCH_CPU_X86_FAMILY)
static __m128 v_wrap_virtual_index(__m128 x,
                                   __m128 wave_size,
                                   __m128 inv_wave_size) {
  // Wrap the virtual index |x| to the range 0 to wave_size - 1.  This is done
  // by computing x - floor(x/wave_size)*wave_size.
  //
  // But there's no SSE2 SIMD instruction for this, so we do it the following
  // way.

  // f = truncate(x/wave_size), truncating towards 0.
  const __m128 r = _mm_mul_ps(x, inv_wave_size);
  __m128i f = _mm_cvttps_epi32(r);

  // Note that if r >= 0, then f <= r. But if r < 0, then r <= f, with equality
  // only if r is already an integer.  Hence if r < f, we want to subtract 1
  // from f to get floor(r).

  // cmplt(a,b) returns 0xffffffff (-1) if a < b and 0 if not.  So cmp is -1 or
  // 0 depending on whether r < f, which is what we need to compute floor(r).
  const __m128i cmp = _mm_cmplt_ps(r, _mm_cvtepi32_ps(f));

  // This subtracts 1 if needed to get floor(r).
  f = _mm_add_epi32(f, cmp);

  // Convert back to float, and scale by wave_size.  And finally subtract that
  // from x.
  return _mm_sub_ps(x, _mm_mul_ps(_mm_cvtepi32_ps(f), wave_size));
}

std::tuple<int, double> OscillatorHandler::ProcessKRateVector(
    int n,
    float* dest_p,
    double virtual_read_index,
    float frequency,
    float rate_scale) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;
  float incr = frequency * rate_scale;
  DCHECK_GE(incr, kInterpolate2Point);

  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const __m128 v_wave_size = _mm_set1_ps(periodic_wave_size);
  const __m128 v_inv_wave_size = _mm_set1_ps(1.0f / periodic_wave_size);

  // Mask to use to wrap the read indices to the proper range.
  const __m128i v_read_mask = _mm_set1_epi32(periodic_wave_size - 1);
  const __m128i one = _mm_set1_epi32(1);

  const __m128 v_table_factor = _mm_set1_ps(table_interpolation_factor);

  // The loop processes 4 items at a time, so we need to increment the
  // virtual index by 4*incr each time.
  const __m128 v_incr = _mm_set1_ps(4 * incr);

  // The virtual index vector.  Ideally, to preserve accuracy, we should use
  // (two) packed double vectors for this, but that degrades performance quite a
  // bit.
  __m128 v_virt_index =
      _mm_set_ps(virtual_read_index + 3 * incr, virtual_read_index + 2 * incr,
                 virtual_read_index + incr, virtual_read_index);

  // It's possible that adding the incr above exceeded the bounds, so wrap them
  // if needed.
  v_virt_index =
      v_wrap_virtual_index(v_virt_index, v_wave_size, v_inv_wave_size);

  // Temporary arrays where we can gather up the wave data we need for
  // interpolation.  Align these for best efficiency on older CPUs where aligned
  // access is much faster than unaliged.
  // TODO(1013118): Is there a faster way to do this?
  float sample1_lower[4] __attribute__((aligned(16)));
  float sample2_lower[4] __attribute__((aligned(16)));
  float sample1_higher[4] __attribute__((aligned(16)));
  float sample2_higher[4] __attribute__((aligned(16)));

  int k = 0;
  int n_loops = n / 4;

  for (int loop = 0; loop < n_loops; ++loop, k += 4) {
    // Compute indices for the samples and contain within the valid range.
    const __m128i read_index_0 =
        _mm_and_si128(_mm_cvttps_epi32(v_virt_index), v_read_mask);
    const __m128i read_index_1 =
        _mm_and_si128(_mm_add_epi32(read_index_0, one), v_read_mask);

    // Extract the components of the indices so we can get the samples
    // associated with the lower and higher wave data.
    const uint32_t* r0 = reinterpret_cast<const uint32_t*>(&read_index_0);
    const uint32_t* r1 = reinterpret_cast<const uint32_t*>(&read_index_1);

    // Get the samples from the wave tables and save them in work arrays so we
    // can load them into simd registers.
    for (int m = 0; m < 4; ++m) {
      sample1_lower[m] = lower_wave_data[r0[m]];
      sample2_lower[m] = lower_wave_data[r1[m]];
      sample1_higher[m] = higher_wave_data[r0[m]];
      sample2_higher[m] = higher_wave_data[r1[m]];
    }

    const __m128 s1_low = _mm_load_ps(sample1_lower);
    const __m128 s2_low = _mm_load_ps(sample2_lower);
    const __m128 s1_high = _mm_load_ps(sample1_higher);
    const __m128 s2_high = _mm_load_ps(sample2_higher);

    // Linearly interpolate within each table (lower and higher).
    const __m128 interpolation_factor =
        _mm_sub_ps(v_virt_index, _mm_cvtepi32_ps(read_index_0));
    const __m128 sample_higher = _mm_add_ps(
        s1_high,
        _mm_mul_ps(interpolation_factor, _mm_sub_ps(s2_high, s1_high)));
    const __m128 sample_lower = _mm_add_ps(
        s1_low, _mm_mul_ps(interpolation_factor, _mm_sub_ps(s2_low, s1_low)));

    // Then interpolate between the two tables.
    const __m128 sample = _mm_add_ps(
        sample_higher,
        _mm_mul_ps(v_table_factor, _mm_sub_ps(sample_lower, sample_higher)));

    // WARNING: dest_p may not be aligned!
    _mm_storeu_ps(dest_p + k, sample);

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    v_virt_index = _mm_add_ps(v_virt_index, v_incr);
    v_virt_index =
        v_wrap_virtual_index(v_virt_index, v_wave_size, v_inv_wave_size);
  }

  // There's a bit of round-off above, so update the index more accurately so at
  // least the next render starts over with a more accurate value.
  virtual_read_index += k * incr;
  virtual_read_index -=
      floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;

  return std::make_tuple(k, virtual_read_index);
}
#elif defined(CPU_ARM_NEON)
static float32x4_t v_wrap_virtual_index(float32x4_t x,
                                        float32x4_t wave_size,
                                        float32x4_t inv_wave_size) {
  // r = x/wave_size, f = truncate(r), truncating towards 0
  const float32x4_t r = vmulq_f32(x, inv_wave_size);
  int32x4_t f = vcvtq_s32_f32(r);

  // vcltq_f32 returns returns all 0xfffffff (-1) if a < b and if if not.
  const uint32x4_t cmp = vcltq_f32(r, vcvtq_f32_s32(f));
  f = vaddq_s32(f, static_cast<int32x4_t>(cmp));

  return vsubq_f32(x, vmulq_f32(vcvtq_f32_s32(f), wave_size));
}

std::tuple<int, double> OscillatorHandler::ProcessKRateVector(
    int n,
    float* dest_p,
    double virtual_read_index,
    float frequency,
    float rate_scale) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;
  const float incr = frequency * rate_scale;
  DCHECK_GE(incr, kInterpolate2Point);

  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float32x4_t v_wave_size = vdupq_n_f32(periodic_wave_size);
  const float32x4_t v_inv_wave_size = vdupq_n_f32(1.0f / periodic_wave_size);

  const uint32x4_t v_read_mask = vdupq_n_s32(periodic_wave_size - 1);
  const uint32x4_t v_one = vdupq_n_s32(1);

  const float32x4_t v_table_factor = vdupq_n_f32(table_interpolation_factor);

  const float32x4_t v_incr = vdupq_n_f32(4 * incr);

  float32x4_t v_virt_index = {
      virtual_read_index + 0 * incr, virtual_read_index + 1 * incr,
      virtual_read_index + 2 * incr, virtual_read_index + 3 * incr};

  // Temporary arrsys to hold the read indices so we can access them
  // individually to get the samples needed for interpolation.
  uint32_t r0[4] __attribute__((aligned(16)));
  uint32_t r1[4] __attribute__((aligned(16)));

  // Temporary arrays where we can gather up the wave data we need for
  // interpolation.  Align these for best efficiency on older CPUs where aligned
  // access is much faster than unaliged.  TODO(rtoy): Is there a faster way to
  // do this?
  float sample1_lower[4] __attribute__((aligned(16)));
  float sample2_lower[4] __attribute__((aligned(16)));
  float sample1_higher[4] __attribute__((aligned(16)));
  float sample2_higher[4] __attribute__((aligned(16)));

  // It's possible that adding the incr above exceeded the bounds, so wrap them
  // if needed.
  v_virt_index =
      v_wrap_virtual_index(v_virt_index, v_wave_size, v_inv_wave_size);

  int k = 0;
  int n_loops = n / 4;

  for (int loop = 0; loop < n_loops; ++loop, k += 4) {
    // Compute indices for the samples and contain within the valid range.
    const uint32x4_t read_index_0 =
        vandq_u32(vcvtq_u32_f32(v_virt_index), v_read_mask);
    const uint32x4_t read_index_1 =
        vandq_u32(vaddq_u32(read_index_0, v_one), v_read_mask);

    // Extract the components of the indices so we can get the samples
    // associated with the lower and higher wave data.
    vst1q_u32(r0, read_index_0);
    vst1q_u32(r1, read_index_1);

    for (int m = 0; m < 4; ++m) {
      sample1_lower[m] = lower_wave_data[r0[m]];
      sample2_lower[m] = lower_wave_data[r1[m]];
      sample1_higher[m] = higher_wave_data[r0[m]];
      sample2_higher[m] = higher_wave_data[r1[m]];
    }

    const float32x4_t s1_low = vld1q_f32(sample1_lower);
    const float32x4_t s2_low = vld1q_f32(sample2_lower);
    const float32x4_t s1_high = vld1q_f32(sample1_higher);
    const float32x4_t s2_high = vld1q_f32(sample2_higher);

    const float32x4_t interpolation_factor =
        vsubq_f32(v_virt_index, vcvtq_f32_u32(read_index_0));
    const float32x4_t sample_higher = vaddq_f32(
        s1_high, vmulq_f32(interpolation_factor, vsubq_f32(s2_high, s1_high)));
    const float32x4_t sample_lower = vaddq_f32(
        s1_low, vmulq_f32(interpolation_factor, vsubq_f32(s2_low, s1_low)));
    const float32x4_t sample = vaddq_f32(
        sample_higher,
        vmulq_f32(v_table_factor, vsubq_f32(sample_lower, sample_higher)));

    vst1q_f32(dest_p + k, sample);

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    v_virt_index = vaddq_f32(v_virt_index, v_incr);
    v_virt_index =
        v_wrap_virtual_index(v_virt_index, v_wave_size, v_inv_wave_size);
  }

  // There's a bit of round-off above, so update the index more accurately so at
  // least the next render starts over with a more accurate value.
  virtual_read_index += k * incr;
  virtual_read_index -=
      floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;

  return std::make_tuple(k, virtual_read_index);
}
#else

// Vector operations not supported, so there's nothing to do except return 0 and
// virtual_read_index.  The scalar version will do the necessary processing.
std::tuple<int, double> OscillatorHandler::ProcessKRateVector(
    int n,
    float* dest_p,
    double virtual_read_index,
    float frequency,
    float rate_scale) const {
  DCHECK_GE(frequency * rate_scale, kInterpolate2Point);
  return std::make_tuple(0, virtual_read_index);
}
#endif

double OscillatorHandler::ProcessKRateScalar(int start,
                                             int n,
                                             float* dest_p,
                                             double virtual_read_index,
                                             float frequency,
                                             float rate_scale) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  const unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float incr = frequency * rate_scale;
  DCHECK_GE(incr, kInterpolate2Point);

  for (int k = start; k < n; ++k) {
    // Get indices for the current and next sample, and contain them within the
    // valid range
    const unsigned read_index_0 =
        static_cast<unsigned>(virtual_read_index) & read_index_mask;
    const unsigned read_index_1 = (read_index_0 + 1) & read_index_mask;

    const float sample1_lower = lower_wave_data[read_index_0];
    const float sample2_lower = lower_wave_data[read_index_1];
    const float sample1_higher = higher_wave_data[read_index_0];
    const float sample2_higher = higher_wave_data[read_index_1];

    // Linearly interpolate within each table (lower and higher).
    const float interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index_0;
    const float sample_higher =
        sample1_higher +
        interpolation_factor * (sample2_higher - sample1_higher);
    const float sample_lower =
        sample1_lower + interpolation_factor * (sample2_lower - sample1_lower);

    // Then interpolate between the two tables.
    const float sample = sample_higher + table_interpolation_factor *
                                             (sample_lower - sample_higher);

    dest_p[k] = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  return virtual_read_index;
}

double OscillatorHandler::ProcessKRate(int n,
                                       float* dest_p,
                                       double virtual_read_index) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  const unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  float frequency = frequency_->FinalValue();
  const float detune_scale = DetuneToFrequencyMultiplier(detune_->FinalValue());
  frequency *= detune_scale;
  ClampFrequency(&frequency, 1, Context()->sampleRate() / 2);
  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float rate_scale = periodic_wave_->RateScale();
  const float incr = frequency * rate_scale;

  if (incr >= kInterpolate2Point) {
    int k;
    double v_index = virtual_read_index;

    std::tie(k, v_index) =
        ProcessKRateVector(n, dest_p, v_index, frequency, rate_scale);

    if (k < n) {
      // In typical cases, this won't be run because the number of frames is 128
      // so the vector version will process all the samples.
      v_index =
          ProcessKRateScalar(k, n, dest_p, v_index, frequency, rate_scale);
    }

    // Recompute to reduce round-off introduced when processing the samples
    // above.
    virtual_read_index += n * incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  } else {
    for (int k = 0; k < n; ++k) {
      float sample = DoInterpolation(
          virtual_read_index, fabs(incr), read_index_mask,
          table_interpolation_factor, lower_wave_data, higher_wave_data);

      *dest_p++ = sample;

      // Increment virtual read index and wrap virtualReadIndex into the range
      // 0 -> periodicWaveSize.
      virtual_read_index += incr;
      virtual_read_index -= floor(virtual_read_index * inv_periodic_wave_size) *
                            periodic_wave_size;
    }
  }

  return virtual_read_index;
}

double OscillatorHandler::ProcessARate(int n,
                                       float* dest_p,
                                       double virtual_read_index,
                                       float* phase_increments) const {
  float rate_scale = periodic_wave_->RateScale();
  float inv_rate_scale = 1 / rate_scale;
  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  for (int k = 0; k < n; ++k) {
    float incr = *phase_increments++;

    float frequency = inv_rate_scale * incr;
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);

    float sample = DoInterpolation(virtual_read_index, fabs(incr),
                                   read_index_mask, table_interpolation_factor,
                                   lower_wave_data, higher_wave_data);

    *dest_p++ = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  return virtual_read_index;
}

void OscillatorHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  DCHECK_LE(frames_to_process, phase_increments_.size());

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
    // Too bad - the tryLock() failed. We must be in the middle of changing
    // wave-tables.
    output_bus->Zero();
    return;
  }

  // We must access m_periodicWave only inside the lock.
  if (!periodic_wave_.Get()) {
    output_bus->Zero();
    return;
  }

  size_t quantum_frame_offset;
  uint32_t non_silent_frames_to_process;
  double start_frame_offset;

  std::tie(quantum_frame_offset, non_silent_frames_to_process,
           start_frame_offset) =
      UpdateSchedulingInfo(frames_to_process, output_bus);

  if (!non_silent_frames_to_process) {
    output_bus->Zero();
    return;
  }

  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();

  float* dest_p = output_bus->Channel(0)->MutableData();

  DCHECK_LE(quantum_frame_offset, frames_to_process);

  // We keep virtualReadIndex double-precision since we're accumulating values.
  double virtual_read_index = virtual_read_index_;

  float rate_scale = periodic_wave_->RateScale();
  bool has_sample_accurate_values =
      CalculateSampleAccuratePhaseIncrements(frames_to_process);

  float frequency = 0;
  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  if (!has_sample_accurate_values) {
    frequency = frequency_->FinalValue();
    float detune = detune_->FinalValue();
    float detune_scale = DetuneToFrequencyMultiplier(detune);
    frequency *= detune_scale;
    ClampFrequency(&frequency, 1, Context()->sampleRate() / 2);
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);
  }

  float* phase_increments = phase_increments_.Data();

  // Start rendering at the correct offset.
  dest_p += quantum_frame_offset;
  int n = non_silent_frames_to_process;

  // If startFrameOffset is not 0, that means the oscillator doesn't actually
  // start at quantumFrameOffset, but just past that time.  Adjust destP and n
  // to reflect that, and adjust virtualReadIndex to start the value at
  // startFrameOffset.
  if (start_frame_offset > 0) {
    ++dest_p;
    --n;
    virtual_read_index += (1 - start_frame_offset) * frequency * rate_scale;
    DCHECK(virtual_read_index < periodic_wave_size);
  } else if (start_frame_offset < 0) {
    virtual_read_index = -start_frame_offset * frequency * rate_scale;
  }

  if (has_sample_accurate_values) {
    virtual_read_index =
        ProcessARate(n, dest_p, virtual_read_index, phase_increments);
  } else {
    virtual_read_index = ProcessKRate(n, dest_p, virtual_read_index);
  }

  virtual_read_index_ = virtual_read_index;

  output_bus->ClearSilentFlag();
}

void OscillatorHandler::SetPeriodicWave(PeriodicWave* periodic_wave) {
  DCHECK(IsMainThread());
  DCHECK(periodic_wave);

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  periodic_wave_ = periodic_wave;
  type_ = CUSTOM;
}

bool OscillatorHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished() || !periodic_wave_.Get();
}

void OscillatorHandler::HandleStoppableSourceNode() {
  double now = Context()->currentTime();

  // If we know the end time, and the source was started and the current time is
  // definitely past the end time, we can stop this node.  (This handles the
  // case where the this source is not connected to the destination and we want
  // to stop it.)
  if (end_time_ != kUnknownTime && IsPlayingOrScheduled() &&
      now >= end_time_ + kExtraStopFrames / Context()->sampleRate()) {
    Finish();
  }
}

// ----------------------------------------------------------------

OscillatorNode::OscillatorNode(BaseAudioContext& context,
                               const String& oscillator_type,
                               PeriodicWave* wave_table)
    : AudioScheduledSourceNode(context),
      // Use musical pitch standard A440 as a default.
      frequency_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeOscillatorFrequency,
                             440,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             -context.sampleRate() / 2,
                             context.sampleRate() / 2)),
      // Default to no detuning.
      detune_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeOscillatorDetune,
                             0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             -1200 * log2f(std::numeric_limits<float>::max()),
                             1200 * log2f(std::numeric_limits<float>::max()))),
      periodic_wave_(wave_table) {
  SetHandler(OscillatorHandler::Create(
      *this, context.sampleRate(), oscillator_type, wave_table,
      frequency_->Handler(), detune_->Handler()));
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext& context,
                                       const String& oscillator_type,
                                       PeriodicWave* wave_table,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<OscillatorNode>(context, oscillator_type,
                                              wave_table);
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext* context,
                                       const OscillatorOptions* options,
                                       ExceptionState& exception_state) {
  if (options->type() == "custom" && !options->hasPeriodicWave()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A PeriodicWave must be specified if the type is set to \"custom\"");
    return nullptr;
  }

  // TODO(crbug.com/1070871): Use periodicWaveOr(nullptr).
  OscillatorNode* node =
      Create(*context, options->type(),
             options->hasPeriodicWave() ? options->periodicWave() : nullptr,
             exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->detune()->setValue(options->detune());
  node->frequency()->setValue(options->frequency());

  return node;
}

void OscillatorNode::Trace(Visitor* visitor) const {
  visitor->Trace(frequency_);
  visitor->Trace(detune_);
  visitor->Trace(periodic_wave_);
  AudioScheduledSourceNode::Trace(visitor);
}

OscillatorHandler& OscillatorNode::GetOscillatorHandler() const {
  return static_cast<OscillatorHandler&>(Handler());
}

String OscillatorNode::type() const {
  return GetOscillatorHandler().GetType();
}

void OscillatorNode::setType(const String& type,
                             ExceptionState& exception_state) {
  GetOscillatorHandler().SetType(type, exception_state);
}

AudioParam* OscillatorNode::frequency() {
  return frequency_;
}

AudioParam* OscillatorNode::detune() {
  return detune_;
}

void OscillatorNode::setPeriodicWave(PeriodicWave* wave) {
  periodic_wave_ = wave;
  GetOscillatorHandler().SetPeriodicWave(wave);
}

void OscillatorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(detune_);
  GraphTracer().DidCreateAudioParam(frequency_);
}

void OscillatorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(detune_);
  GraphTracer().WillDestroyAudioParam(frequency_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
