// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"

#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"

#include <xmmintrin.h>

namespace blink {

static __m128 WrapVirtualIndexVector(__m128 x,
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
  const __m128i cmp =
      reinterpret_cast<__m128i>(_mm_cmplt_ps(r, _mm_cvtepi32_ps(f)));

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
      WrapVirtualIndexVector(v_virt_index, v_wave_size, v_inv_wave_size);

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
        WrapVirtualIndexVector(v_virt_index, v_wave_size, v_inv_wave_size);
  }

  // There's a bit of round-off above, so update the index more accurately so at
  // least the next render starts over with a more accurate value.
  virtual_read_index += k * incr;
  virtual_read_index -=
      floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;

  return std::make_tuple(k, virtual_read_index);
}

}  // namespace blink
