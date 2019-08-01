// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"

#include "base/win/core_winrt_util.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

namespace {
using ABI::Windows::Devices::Sensors::ILightSensor;
using ABI::Windows::Devices::Sensors::ILightSensorReading;
using ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs;
using ABI::Windows::Devices::Sensors::ILightSensorStatics;
using ABI::Windows::Devices::Sensors::LightSensor;
using ABI::Windows::Devices::Sensors::LightSensorReadingChangedEventArgs;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
}  // namespace

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtFactory::Create(mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      return PlatformSensorReaderWinrtLightSensor::Create();
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::PlatformSensorReaderWinrtBase() {
  get_sensor_factory_callback_ =
      base::Bind([](ISensorWinrtStatics** sensor_factory) -> HRESULT {
        return base::win::GetActivationFactory<ISensorWinrtStatics,
                                               runtime_class_id>(
            sensor_factory);
      });
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
void PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::SetClient(Client* client) {
  base::AutoLock autolock(lock_);
  client_ = client;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
template <class ISensorReading>
HRESULT PlatformSensorReaderWinrtBase<runtime_class_id,
                                      ISensorWinrtStatics,
                                      ISensorWinrtClass,
                                      ISensorReadingChangedHandler,
                                      ISensorReadingChangedEventArgs>::
    ConvertSensorReadingTimeStamp(ComPtr<ISensorReading> sensor_reading,
                                  base::TimeDelta* timestamp_delta) {
  DateTime timestamp;
  HRESULT hr = sensor_reading->get_Timestamp(&timestamp);
  if (FAILED(hr))
    return hr;

  *timestamp_delta = base::TimeDelta::FromWinrtDateTime(timestamp);

  return S_OK;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
SensorWinrtCreateFailure
PlatformSensorReaderWinrtBase<runtime_class_id,
                              ISensorWinrtStatics,
                              ISensorWinrtClass,
                              ISensorReadingChangedHandler,
                              ISensorReadingChangedEventArgs>::Initialize() {
  ComPtr<ISensorWinrtStatics> sensor_statics;

  HRESULT hr = get_sensor_factory_callback_.Run(&sensor_statics);

  if (FAILED(hr))
    return SensorWinrtCreateFailure::kErrorISensorWinrtStaticsActivationFailed;

  if (FAILED(sensor_statics->GetDefault(&sensor_)))
    return SensorWinrtCreateFailure::kErrorGetDefaultSensorFailed;

  // GetDefault() returns null if the sensor does not exist
  if (!sensor_)
    return SensorWinrtCreateFailure::kErrorDefaultSensorNull;

  minimum_report_interval_ = GetMinimumReportIntervalFromSensor();

  if (minimum_report_interval_.is_zero())
    return SensorWinrtCreateFailure::kErrorGetMinReportIntervalFailed;

  return SensorWinrtCreateFailure::kOk;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
base::TimeDelta PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::GetMinimumReportIntervalFromSensor() {
  UINT32 minimum_report_interval_ms = 0;
  HRESULT hr = sensor_->get_MinimumReportInterval(&minimum_report_interval_ms);

  // Failing to query is not fatal, consumer should be able to gracefully
  // handle a 0 return.
  if (FAILED(hr)) {
    DLOG(WARNING) << "Failed to query sensor minimum report interval: "
                  << logging::SystemErrorCodeToString(hr);

    return base::TimeDelta();
  }

  return base::TimeDelta::FromMilliseconds(minimum_report_interval_ms);
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
base::TimeDelta PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::GetMinimalReportingInterval() const {
  return minimum_report_interval_;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
bool PlatformSensorReaderWinrtBase<runtime_class_id,
                                   ISensorWinrtStatics,
                                   ISensorWinrtClass,
                                   ISensorReadingChangedHandler,
                                   ISensorReadingChangedEventArgs>::
    StartSensor(const PlatformSensorConfiguration& configuration) {
  base::AutoLock autolock(lock_);

  if (!reading_callback_token_) {
    // Convert from frequency to interval in milliseconds since that is
    // what the Windows.Devices.Sensors API uses.
    unsigned int interval =
        (1 / configuration.frequency()) * base::Time::kMillisecondsPerSecond;

    auto hr = sensor_->put_ReportInterval(interval);

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to set report interval: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }

    auto reading_changed_handler = Callback<ISensorReadingChangedHandler>(
        this, &PlatformSensorReaderWinrtBase::OnReadingChangedCallback);

    EventRegistrationToken event_token;
    hr = sensor_->add_ReadingChanged(reading_changed_handler.Get(),
                                     &event_token);

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to add reading callback handler: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }

    reading_callback_token_ = event_token;
  }

  return true;
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
void PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::StopSensor() {
  base::AutoLock autolock(lock_);

  if (reading_callback_token_) {
    HRESULT hr =
        sensor_->remove_ReadingChanged(reading_callback_token_.value());

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to remove ALS reading callback handler: "
                  << logging::SystemErrorCodeToString(hr);
    }

    reading_callback_token_ = base::nullopt;
  }
}

template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
PlatformSensorReaderWinrtBase<
    runtime_class_id,
    ISensorWinrtStatics,
    ISensorWinrtClass,
    ISensorReadingChangedHandler,
    ISensorReadingChangedEventArgs>::~PlatformSensorReaderWinrtBase() {
  StopSensor();
}

// static
std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorReaderWinrtLightSensor::Create() {
  auto light_sensor = std::make_unique<PlatformSensorReaderWinrtLightSensor>();

  auto initialize_return = light_sensor->Initialize();

  // Failing to query the default report interval is not a fatal error. The
  // consumer (PlatformSensorWin) should be able to handle this and return a
  // default report interval instead.
  if ((initialize_return == SensorWinrtCreateFailure::kOk) ||
      (initialize_return ==
       SensorWinrtCreateFailure::kErrorGetMinReportIntervalFailed)) {
    return light_sensor;
  } else {
    return nullptr;
  }
}

HRESULT PlatformSensorReaderWinrtLightSensor::OnReadingChangedCallback(
    ILightSensor* light_sensor,
    ILightSensorReadingChangedEventArgs* reading_changed_args) {
  ComPtr<ILightSensorReading> light_sensor_reading;

  HRESULT hr = reading_changed_args->get_Reading(&light_sensor_reading);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get the sensor reading: "
                << logging::SystemErrorCodeToString(hr);
    // Failing to parse a reading sample should not be fatal so always
    // return S_OK.
    return S_OK;
  }

  float lux = 0.0f;
  hr = light_sensor_reading->get_IlluminanceInLux(&lux);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get the lux level: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  base::TimeDelta timestamp_delta;
  hr = ConvertSensorReadingTimeStamp(light_sensor_reading, &timestamp_delta);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get sensor reading timestamp: "
                << logging::SystemErrorCodeToString(hr);
    return S_OK;
  }

  SensorReading reading{};
  reading.als.value = lux;
  reading.als.timestamp = timestamp_delta.InSecondsF();
  client_->OnReadingUpdated(reading);

  return S_OK;
}

}  // namespace device