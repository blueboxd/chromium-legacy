// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"

#include <objbase.h>

#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_com_initializer.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

static constexpr unsigned long kExpectedMinimumReportInterval = 213;
static constexpr double kExpectedReportFrequencySet = 25.0;
static constexpr unsigned long kExpectedReportIntervalSet = 40;

// Base mock class for the Windows::Devices::Sensors::I*SensorReadings
template <class ISensorReading>
class FakeSensorReadingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorReading> {
 public:
  FakeSensorReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp)
      : time_stamp_(time_stamp) {}

  IFACEMETHOD(get_Timestamp)
  (ABI::Windows::Foundation::DateTime* time_stamp) override {
    *time_stamp = time_stamp_;
    return get_timestamp_return_code_;
  }

  void SetGetTimestampReturnCode(HRESULT return_code) {
    get_timestamp_return_code_ = return_code;
  }

 protected:
  ABI::Windows::Foundation::DateTime time_stamp_;
  HRESULT get_timestamp_return_code_ = S_OK;
};

class FakeLightSensorReadingWinrt
    : public FakeSensorReadingWinrt<
          ABI::Windows::Devices::Sensors::ILightSensorReading> {
 public:
  FakeLightSensorReadingWinrt(ABI::Windows::Foundation::DateTime time_stamp,
                              float lux)
      : FakeSensorReadingWinrt(time_stamp), lux_(lux) {}

  ~FakeLightSensorReadingWinrt() override = default;

  IFACEMETHOD(get_IlluminanceInLux)(float* lux) override {
    *lux = lux_;
    return get_illuminance_in_lux_return_code_;
  }

  void SetGetIlluminanceInLuxReturnCode(HRESULT return_code) {
    get_illuminance_in_lux_return_code_ = return_code;
  }

 private:
  float lux_;
  HRESULT get_illuminance_in_lux_return_code_ = S_OK;
};

// Mock class for Windows::Devices::Sensors::ISensorReadingChangedEventArgs,
// allows reading changed events to return mock sensor readings.
template <class ISensorReading, class ISensorReadingChangedEventArgs>
class FakeSensorReadingChangedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorReadingChangedEventArgs> {
 public:
  FakeSensorReadingChangedEventArgsWinrt(
      const Microsoft::WRL::ComPtr<ISensorReading>& reading)
      : reading_(reading) {}

  ~FakeSensorReadingChangedEventArgsWinrt() override = default;

  IFACEMETHOD(get_Reading)
  (ISensorReading** reading) override { return reading_.CopyTo(reading); }

 private:
  Microsoft::WRL::ComPtr<ISensorReading> reading_;
};

// Mock client used to receive sensor data callbacks
class MockClient : public PlatformSensorReaderWinBase::Client {
 public:
  MOCK_METHOD1(OnReadingUpdated, void(const SensorReading& reading));
  MOCK_METHOD0(OnSensorError, void());

 protected:
  ~MockClient() override = default;
};

// Base mock class for Windows.Devices.Sensors.ISensor*, injected into
// PlatformSensorReaderWinrt* to mock out the underlying
// Windows.Devices.Sensors dependency.
template <class ISensor,
          class Sensor,
          class ISensorReading,
          class ISensorReadingChangedEventArgs,
          class SensorReadingChangedEventArgs>
class FakeSensorWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensor> {
 public:
  ~FakeSensorWinrt() override = default;

  IFACEMETHOD(GetCurrentReading)
  (ISensorReading** ppReading) override { return E_NOTIMPL; }

  IFACEMETHOD(get_MinimumReportInterval)(UINT32* pValue) override {
    *pValue = kExpectedMinimumReportInterval;
    return get_minimum_report_interval_return_code_;
  }

  IFACEMETHOD(get_ReportInterval)(UINT32* pValue) override { return E_NOTIMPL; }

  IFACEMETHOD(put_ReportInterval)(UINT32 value) override {
    EXPECT_EQ(value, kExpectedReportIntervalSet);
    return put_report_interval_return_code_;
  }

  IFACEMETHOD(add_ReadingChanged)
  (ABI::Windows::Foundation::ITypedEventHandler<Sensor*,
                                                SensorReadingChangedEventArgs*>*
       pHandler,
   EventRegistrationToken* pToken) override {
    handler_ = pHandler;
    return add_reading_changed_return_code_;
  }

  IFACEMETHOD(remove_ReadingChanged)(EventRegistrationToken iToken) override {
    handler_.Reset();
    return remove_reading_changed_return_code_;
  }

  // Makes any clients registered via add_ReadingChanged() to trigger with
  // the given sensor reading.
  void TriggerFakeSensorReading(
      Microsoft::WRL::ComPtr<ISensorReading> reading) {
    EXPECT_TRUE(handler_);
    Microsoft::WRL::ComPtr<ISensorReadingChangedEventArgs> reading_event_args =
        Microsoft::WRL::Make<FakeSensorReadingChangedEventArgsWinrt<
            ISensorReading, ISensorReadingChangedEventArgs>>(reading);
    EXPECT_HRESULT_SUCCEEDED(handler_->Invoke(this, reading_event_args.Get()));
  }

  // Returns true if any clients are registered for readings via
  // add_ReadingChanged(), false otherwise.
  bool IsSensorStarted() { return handler_; }

  void SetGetMinimumReportIntervalReturnCode(HRESULT return_code) {
    get_minimum_report_interval_return_code_ = return_code;
  }

  void SetPutReportIntervalReturnCode(HRESULT return_code) {
    put_report_interval_return_code_ = return_code;
  }

  void SetAddReadingChangedReturnCode(HRESULT return_code) {
    add_reading_changed_return_code_ = return_code;
  }

  void SetRemoveReadingChangedReturnCode(HRESULT return_code) {
    remove_reading_changed_return_code_ = return_code;
  }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      Sensor*,
      SensorReadingChangedEventArgs*>>
      handler_;

  HRESULT put_report_interval_return_code_ = S_OK;
  HRESULT get_minimum_report_interval_return_code_ = S_OK;
  HRESULT add_reading_changed_return_code_ = S_OK;
  HRESULT remove_reading_changed_return_code_ = S_OK;
};

template <class ISensorStatics,
          class ISensor,
          class Sensor,
          class ISensorReading,
          class ISensorReadingChangedEventArgs,
          class SensorReadingChangedEventArgs>
class FakeSensorFactoryWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ISensorStatics> {
 public:
  FakeSensorFactoryWinrt() {
    fake_sensor_ = Microsoft::WRL::Make<FakeSensorWinrt<
        ABI::Windows::Devices::Sensors::ILightSensor,
        ABI::Windows::Devices::Sensors::LightSensor,
        ABI::Windows::Devices::Sensors::ILightSensorReading,
        ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
        ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  }
  ~FakeSensorFactoryWinrt() override = default;

  IFACEMETHOD(GetDefault)(ISensor** ppResult) override {
    if (fake_sensor_ && SUCCEEDED(get_default_return_code_)) {
      return fake_sensor_.CopyTo(ppResult);
    }
    return get_default_return_code_;
  }

  Microsoft::WRL::ComPtr<FakeSensorWinrt<
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>
      fake_sensor_;

  void SetRemoveReadingChangedReturnCode(HRESULT return_code) {
    get_default_return_code_ = return_code;
  }

 private:
  HRESULT get_default_return_code_ = S_OK;
};

class PlatformSensorReaderTestWinrt : public testing::Test {
 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::win::ScopedCOMInitializer scoped_com_initializer_;
};

// Tests that PlatformSensorReaderWinrtBase returns the expected error
// if it could not create the underlying sensor.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorCreate) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  fake_sensor_factory->fake_sensor_ = nullptr;
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(),
            SensorWinrtCreateFailure::kErrorDefaultSensorNull);

  fake_sensor_factory->SetRemoveReadingChangedReturnCode(E_FAIL);
  EXPECT_EQ(sensor->Initialize(),
            SensorWinrtCreateFailure::kErrorGetDefaultSensorFailed);

  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return E_FAIL; }));
  EXPECT_EQ(
      sensor->Initialize(),
      SensorWinrtCreateFailure::kErrorISensorWinrtStaticsActivationFailed);
}

// Tests that PlatformSensorReaderWinrtBase returns the right
// minimum report interval.
TEST_F(PlatformSensorReaderTestWinrt, SensorMinimumReportInterval) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  EXPECT_EQ(sensor->GetMinimalReportingInterval().InMilliseconds(),
            kExpectedMinimumReportInterval);
}

// Tests that PlatformSensorReaderWinrtBase returns a 0 report interval
// when it fails to query the sensor.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorMinimumReportInterval) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  fake_sensor->SetGetMinimumReportIntervalReturnCode(E_FAIL);
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(),
            SensorWinrtCreateFailure::kErrorGetMinReportIntervalFailed);

  EXPECT_EQ(sensor->GetMinimalReportingInterval().InMilliseconds(), 0);
}

// Tests that PlatformSensorReaderWinrtBase converts the timestamp correctly
TEST_F(PlatformSensorReaderTestWinrt, SensorTimestampConversion) {
  static constexpr double expectedTimestampDeltaSecs = 19.0;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();

  double lastReportedTimestamp = 0.0;
  EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
      .WillRepeatedly(testing::Invoke([&](const SensorReading& reading) {
        lastReportedTimestamp = reading.als.timestamp;
        EXPECT_EQ(reading.als.value, 0.0f);
      }));

  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  // Trigger a sensor reading at time 0 (epoch), converted timestamp should
  // also be 0.
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Sensors::ILightSensorReading>
      reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
          ABI::Windows::Foundation::DateTime{}, 0.0f);
  fake_sensor->TriggerFakeSensorReading(reading);
  EXPECT_EQ(lastReportedTimestamp, 0);

  auto second_timestamp =
      base::TimeDelta::FromSeconds(expectedTimestampDeltaSecs)
          .ToWinrtDateTime();
  reading =
      Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(second_timestamp, 0.0f);
  fake_sensor->TriggerFakeSensorReading(reading);

  // Verify the reported time stamp has ticked forward
  // expectedTimestampDeltaSecs
  EXPECT_EQ(lastReportedTimestamp, expectedTimestampDeltaSecs);
}

// Tests that PlatformSensorReaderWinrtBase starts and stops the
// underlying sensor when Start() and Stop() are called.
TEST_F(PlatformSensorReaderTestWinrt, StartStopSensorCallbacks) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  // Calling Start() contiguously should not cause any errors/exceptions
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  sensor->StopSensor();
  EXPECT_FALSE(fake_sensor->IsSensorStarted());

  // Calling Stop() contiguously should not cause any errors/exceptions
  sensor->StopSensor();
  EXPECT_FALSE(fake_sensor->IsSensorStarted());
}

// Tests that PlatformSensorReaderWinrtBase stops the underlying sensor
// even if Start() is called without a following Stop() upon destruction.
TEST_F(PlatformSensorReaderTestWinrt, StartWithoutStopSensorCallbacks) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));
  EXPECT_TRUE(fake_sensor->IsSensorStarted());

  sensor.reset();
  EXPECT_FALSE(fake_sensor->IsSensorStarted());
}

// Tests that PlatformSensorReaderWinrtBase::StartSensor() returns false
// when setting the report interval or registering for sensor events fails.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorStart) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  fake_sensor->SetPutReportIntervalReturnCode(E_FAIL);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_FALSE(sensor->StartSensor(sensor_config));

  fake_sensor->SetPutReportIntervalReturnCode(S_OK);
  fake_sensor->SetAddReadingChangedReturnCode(E_FAIL);

  EXPECT_FALSE(sensor->StartSensor(sensor_config));
}

// Tests that PlatformSensorReaderWinrtBase::StopSensor() swallows
// unregister sensor change errors.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorStop) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  fake_sensor->SetRemoveReadingChangedReturnCode(E_FAIL);

  sensor->StopSensor();
}

// Tests that PlatformSensorReaderWinrtLightSensor does not notify the
// client if an error occurs during sensor sample parsing.
TEST_F(PlatformSensorReaderTestWinrt, FailedSensorSampleParse) {
  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();

  // This test relies on the NiceMock to work, if the sensor notifies
  // the client despite not being able to parse the sensor sample then
  // the NiceMock will throw an error as no EXPECT_CALL has been set.
  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, 0.0f);

  reading->SetGetTimestampReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);

  reading->SetGetTimestampReturnCode(S_OK);
  reading->SetGetIlluminanceInLuxReturnCode(E_FAIL);
  fake_sensor->TriggerFakeSensorReading(reading);
}

// Tests that PlatformSensorReaderWinrtLightSensor notifies the client
// and gives it the correct sensor data when a new sensor sample arrives.
TEST_F(PlatformSensorReaderTestWinrt, SensorClientNotification) {
  static constexpr float expected_lux = 123.0f;

  auto fake_sensor_factory = Microsoft::WRL::Make<FakeSensorFactoryWinrt<
      ABI::Windows::Devices::Sensors::ILightSensorStatics,
      ABI::Windows::Devices::Sensors::ILightSensor,
      ABI::Windows::Devices::Sensors::LightSensor,
      ABI::Windows::Devices::Sensors::ILightSensorReading,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs,
      ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs>>();
  auto fake_sensor = fake_sensor_factory->fake_sensor_;

  auto sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();
  sensor->InitForTests(base::BindLambdaForTesting(
      [&](ABI::Windows::Devices::Sensors::ILightSensorStatics** sensor_factory)
          -> HRESULT { return fake_sensor_factory.CopyTo(sensor_factory); }));
  EXPECT_EQ(sensor->Initialize(), SensorWinrtCreateFailure::kOk);

  auto mock_client = std::make_unique<testing::NiceMock<MockClient>>();

  EXPECT_CALL(*mock_client, OnReadingUpdated(::testing::_))
      .WillOnce(testing::Invoke([&](const SensorReading& reading) {
        EXPECT_EQ(expected_lux, reading.als.value);
      }));

  sensor->SetClient(mock_client.get());

  PlatformSensorConfiguration sensor_config(kExpectedReportFrequencySet);
  EXPECT_TRUE(sensor->StartSensor(sensor_config));

  auto reading = Microsoft::WRL::Make<FakeLightSensorReadingWinrt>(
      ABI::Windows::Foundation::DateTime{}, expected_lux);
  fake_sensor->TriggerFakeSensorReading(reading);

  sensor->StopSensor();
}

}  // namespace device