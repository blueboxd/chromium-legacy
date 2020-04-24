// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/ambient_light_sensor.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_test_utils.h"

namespace blink {

namespace {

class MockSensorProxyObserver
    : public GarbageCollected<MockSensorProxyObserver>,
      public SensorProxy::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(MockSensorProxyObserver);

 public:
  virtual ~MockSensorProxyObserver() = default;

  // Synchronously waits for OnSensorReadingChanged() to be called.
  void WaitForOnSensorReadingChanged() {
    sensor_reading_changed_run_loop_.emplace();
    sensor_reading_changed_run_loop_->Run();
  }

  // Synchronously waits for OnSensorInitialized() to be called.
  void WaitForSensorInitialization() {
    sensor_initialized_run_loop_.emplace();
    sensor_initialized_run_loop_->Run();
  }

  // SensorProxy::Observer overrides.
  void OnSensorInitialized() override {
    if (sensor_initialized_run_loop_.has_value() &&
        sensor_initialized_run_loop_->running()) {
      sensor_initialized_run_loop_->Quit();
    }
  }

  void OnSensorReadingChanged() override {
    if (sensor_reading_changed_run_loop_.has_value() &&
        sensor_reading_changed_run_loop_->running()) {
      sensor_reading_changed_run_loop_->Quit();
    }
  }

 private:
  base::Optional<base::RunLoop> sensor_initialized_run_loop_;
  base::Optional<base::RunLoop> sensor_reading_changed_run_loop_;
};

}  // namespace

TEST(AmbientLightSensorTest, IlluminanceInStoppedSensor) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);

  EXPECT_FALSE(sensor->illuminance().has_value());
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, IlluminanceInSensorWithoutReading) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kActivate);

  EXPECT_FALSE(sensor->illuminance().has_value());
  EXPECT_FALSE(sensor->hasReading());
}

TEST(AmbientLightSensorTest, IlluminanceRounding) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kActivate);
  EXPECT_FALSE(sensor->hasReading());

  // At this point, we have received an 'activate' event, so the sensor is
  // initialized and it is connected to a SensorProxy that we can retrieve
  // here. We then attach a new SensorProxy::Observer that we use to
  // synchronously wait for OnSensorReadingChanged() to be called. Even though
  // the order that each observer is notified is arbitrary, we know that by the
  // time we get to the next call here all observers will have been called.
  auto* sensor_proxy =
      SensorProviderProxy::From(
          To<LocalDOMWindow>(context.GetExecutionContext()))
          ->GetSensorProxy(device::mojom::blink::SensorType::AMBIENT_LIGHT);
  ASSERT_NE(sensor_proxy, nullptr);
  auto* mock_observer = MakeGarbageCollected<MockSensorProxyObserver>();
  sensor_proxy->AddObserver(mock_observer);

  auto* event_counter = MakeGarbageCollected<SensorTestUtils::EventCounter>();
  sensor->addEventListener(event_type_names::kReading, event_counter);

  // Go from no reading to 24. This will cause a new "reading" event to be
  // emitted, and the rounding will cause illuminance() to return 0.
  context.sensor_provider()->UpdateAmbientLightSensorData(24);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(24, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(0, sensor->illuminance().value());

  // Go from 24 to 35. The difference is not significant enough, so we will not
  // emit any "reading" event or store the new raw reading, as if the new
  // reading had never existed.
  context.sensor_provider()->UpdateAmbientLightSensorData(35);
  mock_observer->WaitForOnSensorReadingChanged();
  EXPECT_EQ(24, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(0, sensor->illuminance().value());

  // Go from 24 to 49. The difference is significant enough, so we will emit a
  // new "reading" event, update our raw reading and return a rounded value of
  // 50 in illuminance().
  context.sensor_provider()->UpdateAmbientLightSensorData(49);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(49, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(50, sensor->illuminance().value());

  // Go from 49 to 35. The difference is not significant enough, so we will not
  // emit any "reading" event or store the new raw reading, as if the new
  // reading had never existed.
  context.sensor_provider()->UpdateAmbientLightSensorData(35);
  mock_observer->WaitForOnSensorReadingChanged();
  EXPECT_EQ(49, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(50, sensor->illuminance().value());

  // Go from 49 to 24. The difference is significant enough, so we will emit a
  // new "reading" event, update our raw reading and return a rounded value of
  // 0 in illuminance().
  context.sensor_provider()->UpdateAmbientLightSensorData(24);
  mock_observer->WaitForOnSensorReadingChanged();
  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);
  EXPECT_EQ(24, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(0, sensor->illuminance().value());

  // Make sure there were no stray "reading" events besides those we expected
  // above.
  EXPECT_EQ(3U, event_counter->event_count());
}

TEST(AmbientLightSensorTest, PlatformSensorReadingsBeforeActivation) {
  SensorTestContext context;
  NonThrowableExceptionState exception_state;

  auto* sensor = AmbientLightSensor::Create(context.GetExecutionContext(),
                                            exception_state);
  sensor->start();

  auto* sensor_proxy =
      SensorProviderProxy::From(
          To<LocalDOMWindow>(context.GetExecutionContext()))
          ->GetSensorProxy(device::mojom::blink::SensorType::AMBIENT_LIGHT);
  ASSERT_NE(sensor_proxy, nullptr);
  auto* mock_observer = MakeGarbageCollected<MockSensorProxyObserver>();
  sensor_proxy->AddObserver(mock_observer);

  // Instead of waiting for SensorProxy::Observer::OnSensorReadingChanged(), we
  // wait for OnSensorInitialized(), which happens earlier. The platform may
  // start sending readings and calling OnSensorReadingChanged() at any moment
  // from this point on.
  // This test verifies AmbientLightSensor::OnSensorReadingChanged() is able to
  // handle the case of it being called before Sensor itself has transitioned to
  // a fully activated state.
  mock_observer->WaitForSensorInitialization();
  context.sensor_provider()->UpdateAmbientLightSensorData(42);
  ASSERT_FALSE(sensor->IsActivated());
  EXPECT_FALSE(sensor->illuminance().has_value());

  SensorTestUtils::WaitForEvent(sensor, event_type_names::kReading);

  EXPECT_EQ(42, sensor->latest_reading_);
  ASSERT_TRUE(sensor->illuminance().has_value());
  EXPECT_EQ(50, sensor->illuminance().value());
}

}  // namespace blink
