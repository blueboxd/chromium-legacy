// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterations = 20;
static const int kSampleRate = 48000;

template <typename T, class SampleTraits>
void RunInterleaveBench(AudioBus* bus,
                        const std::string& trace_name,
                        bool to_interleaved_only = false) {
  const int frame_size = bus->frames() * bus->channels();
  std::unique_ptr<T[]> interleaved(new T[frame_size]);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i)
    bus->ToInterleaved<SampleTraits>(bus->frames(), interleaved.get());
  double total_time_milliseconds =
      (base::TimeTicks::Now() - start).InMillisecondsF();
  perf_test::PrintResult("audio_bus_to_interleaved", "", trace_name,
                         total_time_milliseconds / kBenchmarkIterations, "ms",
                         true);

  if (to_interleaved_only)
    return;

  start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i)
    bus->FromInterleaved<SampleTraits>(interleaved.get(), bus->frames());
  total_time_milliseconds = (base::TimeTicks::Now() - start).InMillisecondsF();
  perf_test::PrintResult("audio_bus_from_interleaved", "", trace_name,
                         total_time_milliseconds / kBenchmarkIterations, "ms",
                         true);
}

// Benchmark the FromInterleaved() and ToInterleaved() methods.
TEST(AudioBusPerfTest, Interleave) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(2, kSampleRate * 120);
  FakeAudioRenderCallback callback(0.2, kSampleRate);
  callback.Render(base::TimeDelta(), base::TimeTicks::Now(), 0, bus.get());

  // Only benchmark these two types since they're the only commonly used ones.
  RunInterleaveBench<int16_t, SignedInt16SampleTypeTraits>(bus.get(),
                                                           "int16_t");
  RunInterleaveBench<float, Float32SampleTypeTraits>(bus.get(), "float");
}

TEST(AudioBusPerfTest, DISABLED_ToInterleavedFloat) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(2, kSampleRate * 120);
  FakeAudioRenderCallback callback(0.2, kSampleRate);
  callback.Render(base::TimeDelta(), base::TimeTicks::Now(), 0, bus.get());

  RunInterleaveBench<float, Float32SampleTypeTraits>(
      bus.get(), "to_interleave_float", true);
  RunInterleaveBench<float, Float32SampleTypeTraitsNoClip>(
      bus.get(), "to_interleave_float_no_clip", true);
}

void RunCopyBench(void (AudioBus::*f)(AudioBus*) const, const char* name) {
  // Setup.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(2, kSampleRate * 120);
  std::unique_ptr<AudioBus> dest = AudioBus::Create(2, kSampleRate * 120);
  FakeAudioRenderCallback callback(0.2, kSampleRate);
  callback.Render(base::TimeDelta(), base::TimeTicks::Now(), 0, bus.get());

  // Warmup.
  for (int i = 0; i < kBenchmarkIterations; ++i)
    (bus.get()->*f)(dest.get());

  // Benchmark.
  auto start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i)
    (bus.get()->*f)(dest.get());
  auto total_time_milliseconds =
      (base::TimeTicks::Now() - start).InMillisecondsF();

  perf_test::PrintResult(
      name, "", "", total_time_milliseconds / kBenchmarkIterations, "ms", true);
}

TEST(AudioBusPerfTest, DISABLED_CopyBench) {
  RunCopyBench(&AudioBus::CopyTo, "audio_bus_copy_no_clip");
  RunCopyBench(&AudioBus::CopyAndClipTo, "audio_bus_copy_clip");
}

}  // namespace media
