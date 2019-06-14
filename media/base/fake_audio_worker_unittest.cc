// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_worker.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kTestCallbacks = 5;

class FakeAudioWorkerTest : public testing::Test {
 public:
  FakeAudioWorkerTest()
      : params_(AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 44100, 128),
        fake_worker_(scoped_task_environment_.GetMainThreadTaskRunner(),
                     params_),
        seen_callbacks_(0) {
    time_between_callbacks_ = base::TimeDelta::FromMicroseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
  }

  ~FakeAudioWorkerTest() override = default;

  void CalledByFakeWorker(base::TimeTicks ideal_time, base::TimeTicks now) {
    seen_callbacks_++;
  }

  void RunOnAudioThread() {
    ASSERT_TRUE(scoped_task_environment_.GetMainThreadTaskRunner()
                    ->BelongsToCurrentThread());
    fake_worker_.Start(base::BindRepeating(
        &FakeAudioWorkerTest::CalledByFakeWorker, base::Unretained(this)));
  }

  void RunOnceOnAudioThread() {
    ASSERT_TRUE(scoped_task_environment_.GetMainThreadTaskRunner()
                    ->BelongsToCurrentThread());
    RunOnAudioThread();
    // Start() should immediately post a task to run the callback, so we
    // should end up with only a single callback being run.
    scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&FakeAudioWorkerTest::EndTest,
                                  base::Unretained(this), 1));
  }

  void StopStartOnAudioThread() {
    ASSERT_TRUE(scoped_task_environment_.GetMainThreadTaskRunner()
                    ->BelongsToCurrentThread());
    fake_worker_.Stop();
    RunOnAudioThread();
  }

  void TimeCallbacksOnAudioThread(int callbacks) {
    ASSERT_TRUE(scoped_task_environment_.GetMainThreadTaskRunner()
                    ->BelongsToCurrentThread());

    if (seen_callbacks_ == 0) {
      RunOnAudioThread();
      start_time_ = base::TimeTicks::Now();
    }

    // Keep going until we've seen the requested number of callbacks.
    if (seen_callbacks_ < callbacks) {
      scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                         base::Unretained(this), callbacks),
          time_between_callbacks_ / 2);
    } else {
      end_time_ = base::TimeTicks::Now();
      EndTest(callbacks);
    }
  }

  void EndTest(int callbacks) {
    ASSERT_TRUE(scoped_task_environment_.GetMainThreadTaskRunner()
                    ->BelongsToCurrentThread());
    fake_worker_.Stop();
    EXPECT_LE(callbacks, seen_callbacks_);
    run_loop_.QuitWhenIdle();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::RunLoop run_loop_;
  AudioParameters params_;
  FakeAudioWorker fake_worker_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  base::TimeDelta time_between_callbacks_;
  int seen_callbacks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioWorkerTest);
};

// Ensure the worker runs on the audio thread and fires callbacks.
// TODO(https://crbug.com/945486): Flakily failing on Fuchsia.
#if defined(OS_FUCHSIA)
#define MAYBE_FakeBasicCallback DISABLED_FakeBasicCallback
#else
#define MAYBE_FakeBasicCallback FakeBasicCallback
#endif
TEST_F(FakeAudioWorkerTest, MAYBE_FakeBasicCallback) {
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeAudioWorkerTest::RunOnceOnAudioThread,
                                base::Unretained(this)));
  run_loop_.Run();
}

// Ensure the time between callbacks is sane.
//
// TODO(https://crbug.com/960729): Test is flaky because its behavior depends on
// real wallclock time. Need to mock time to fix this.
TEST_F(FakeAudioWorkerTest, DISABLED_TimeBetweenCallbacks) {
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                     base::Unretained(this), kTestCallbacks));
  run_loop_.Run();

  // There are only (kTestCallbacks - 1) intervals between kTestCallbacks.
  base::TimeDelta actual_time_between_callbacks =
      (end_time_ - start_time_) / (seen_callbacks_ - 1);

  // Ensure callback time is no faster than the expected time between callbacks.
  EXPECT_GE(actual_time_between_callbacks, time_between_callbacks_);

  // Softly check if the callback time is no slower than twice the expected time
  // between callbacks.  Since this test runs on the bots we can't be too strict
  // with the bounds.
  if (actual_time_between_callbacks > 2 * time_between_callbacks_)
    LOG(ERROR) << "Time between fake audio callbacks is too large!";
}

// Ensure Start()/Stop() on the worker doesn't generate too many callbacks. See
// http://crbug.com/159049.
// Flaky test; see crbug.com/974078
TEST_F(FakeAudioWorkerTest, DISABLED_StartStopClearsCallbacks) {
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                     base::Unretained(this), kTestCallbacks));

  // Issue a Stop() / Start() in between expected callbacks to maximize the
  // chance of catching the worker doing the wrong thing.
  scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::StopStartOnAudioThread,
                     base::Unretained(this)),
      time_between_callbacks_ / 2);

  // EndTest() will ensure the proper number of callbacks have occurred.
  run_loop_.Run();
}

}  // namespace media
