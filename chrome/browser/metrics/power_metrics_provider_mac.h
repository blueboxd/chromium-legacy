// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_METRICS_PROVIDER_MAC_H_

#include "components/metrics/metrics_provider.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/time/time.h"

// Records battery power drain in a histogram. To use, repeatedly call
// RecordBatteryDischarge() at regular intervals. The implementation tries to
// correct for cases where the function was called too late or too early to
// avoid recording faulty measurements.
// Example use:
//
//   class UserClass {
//    public:
//     static constexpr base::TimeDelta kRecordingInterval =
//     base::TimeDelta::FromMinutes(1);
//
//     UserClass(){
//       power_drain_recorder_ =
//       std::make_unique<PowerDrainRecorder>(kRecordingInterval);
//     }
//
//     void StartRecording() {
//       timer_.Start(FROM_HERE, kRecordingInterval,
//                    power_drain_recorder_.get(),
//                    &PowerDrainRecorder::RecordBatteryDischarge);
//     }
//
//    private:
//     std::unique_ptr<PowerDrainRecorder> power_drain_recorder_;
//     base::RepeatingTimer timer_;
//   };
//
class PowerDrainRecorder {
 private:
  // Represents the state of the battery at a certain point in time.
  class BatteryState {
   public:
    // Use this constructor to extract the actual battery information using
    // system functions.
    BatteryState();

    // Use this constructor to control the value of all the members.
    BatteryState(int capacity, bool on_battery, base::TimeTicks creation_time);

    // Returns the battery capacity at the time of capture.
    int capacity() const { return capacity_; }

    // Returns true if the system ran on battery power at the time of capture.
    bool on_battery() const { return on_battery_; }

    // Explicitly mark the state object as on battery on not.
    void SetIsOnBattery(bool on_battery);

    // Return the time at which the object was created.
    base::TimeTicks creation_time() const { return creation_time_; }

   private:
    // A portion of the maximal battery capacity of the system.  Units are
    // hundredths of a percent. ie: 99.8742% capacity --> 998742.
    int capacity_ = 0;

    // For the purpose of this class not being on battery power is considered
    // equivalent to charging.
    bool on_battery_ = false;

    // The time at which the battery state capture took place.
    base::TimeTicks creation_time_;
  };

 public:
  // |recording_interval| is the time that is supposed to elapse between calls
  // to RecordBatteryDischarge().
  explicit PowerDrainRecorder(base::TimeDelta recording_interval);
  ~PowerDrainRecorder();

  PowerDrainRecorder(const PowerDrainRecorder& other) = delete;
  PowerDrainRecorder& operator=(const PowerDrainRecorder& other) = delete;

  // Calling this function repeatedly will store the battery discharge that
  // happened between calls in a histogram.
  void RecordBatteryDischarge();

  // Replace the function used to get BatteryState values. Use only for testing
  // to not depend on actual system information.
  void SetGetBatteryStateCallBackForTesting(
      base::RepeatingCallback<BatteryState()>);

 private:
  // Simply creates a BatteryState() object and returns it. Stand-in that can
  // replaced in tests.
  BatteryState GetCurrentBatteryState();

  // Use this callback to get the current battery state. Override in tests to
  // provide test values.
  base::RepeatingCallback<BatteryState()> battery_state_callback_ =
      base::BindRepeating(&PowerDrainRecorder::GetCurrentBatteryState,
                          base::Unretained(this));

  // Default battery state value that makes sure it will not be used towards the
  // first discharge calculation that will happen at some point in the future.
  BatteryState previous_battery_state_ =
      BatteryState(0, false, base::TimeTicks{});

  // Time that should elapse between calls to RecordBatteryDischarge.
  const base::TimeDelta recording_interval_;

  friend class PowerMetricsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest, BatteryDischargeOnPower);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest, BatteryDischargeOnBattery);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCapacityGrew);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsTooEarly);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsEarly);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsTooLate);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsLate);
};

class PowerMetricsProvider : public metrics::MetricsProvider {
 public:
  PowerMetricsProvider();
  ~PowerMetricsProvider() override;

  PowerMetricsProvider(const PowerMetricsProvider& other) = delete;
  PowerMetricsProvider& operator=(const PowerMetricsProvider& other) = delete;

  // metrics::MetricsProvider overrides
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

 private:
  class Impl;
  scoped_refptr<Impl> impl_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_METRICS_PROVIDER_MAC_H_
