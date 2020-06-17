// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/lorgnette_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/dbus/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// It can take a scanner 2+ minutes to return one page at high resolution, so
// extend the D-Bus timeout to 3 minutes.
constexpr base::TimeDelta kScanImageDBusTimeout =
    base::TimeDelta::FromMinutes(3);

}  // namespace

// The LorgnetteManagerClient implementation used in production.
class LorgnetteManagerClientImpl : public LorgnetteManagerClient {
 public:
  LorgnetteManagerClientImpl() = default;
  LorgnetteManagerClientImpl(const LorgnetteManagerClientImpl&) = delete;
  LorgnetteManagerClientImpl& operator=(const LorgnetteManagerClientImpl&) =
      delete;
  ~LorgnetteManagerClientImpl() override = default;

  void ListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kListScannersMethod);
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnListScanners,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteManagerClient override.
  void ScanImageToString(std::string device_name,
                         const ScanProperties& properties,
                         DBusMethodCallback<std::string> callback) override {
    auto scan_data_reader = std::make_unique<ScanDataReader>();
    base::ScopedFD fd = scan_data_reader->Start();

    // Issue the dbus request to scan an image.
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kScanImageMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_name);
    writer.AppendFileDescriptor(fd.get());

    dbus::MessageWriter option_writer(nullptr);
    dbus::MessageWriter element_writer(nullptr);
    writer.OpenArray("{sv}", &option_writer);
    if (!properties.mode.empty()) {
      option_writer.OpenDictEntry(&element_writer);
      element_writer.AppendString(lorgnette::kScanPropertyMode);
      element_writer.AppendVariantOfString(properties.mode);
      option_writer.CloseContainer(&element_writer);
    }
    if (properties.resolution_dpi) {
      option_writer.OpenDictEntry(&element_writer);
      element_writer.AppendString(lorgnette::kScanPropertyResolution);
      element_writer.AppendVariantOfUint32(properties.resolution_dpi);
      option_writer.CloseContainer(&element_writer);
    }
    writer.CloseContainer(&option_writer);

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, kScanImageDBusTimeout.InMilliseconds(),
        base::BindOnce(&LorgnetteManagerClientImpl::OnScanImageComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(scan_data_reader)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    lorgnette_daemon_proxy_ =
        bus->GetObjectProxy(lorgnette::kManagerServiceName,
                            dbus::ObjectPath(lorgnette::kManagerServicePath));
  }

 private:
  // Reads scan data on a blocking sequence.
  class ScanDataReader {
   public:
    // In case of success, std::string holds the read data. Otherwise,
    // nullopt.
    using CompletionCallback =
        base::OnceCallback<void(base::Optional<std::string> data)>;

    ScanDataReader() = default;

    // Creates a pipe to read the scan data from the D-Bus service.
    // Returns a write-side FD.
    base::ScopedFD Start() {
      DCHECK(!pipe_reader_.get());
      DCHECK(!data_.has_value());
      pipe_reader_ = std::make_unique<chromeos::PipeReader>(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(),
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

      return pipe_reader_->StartIO(base::BindOnce(
          &ScanDataReader::OnDataRead, weak_ptr_factory_.GetWeakPtr()));
    }

    // Waits for the data read completion. If it is already done, |callback|
    // will be called synchronously.
    void Wait(CompletionCallback callback) {
      DCHECK(callback_.is_null());
      callback_ = std::move(callback);
      MaybeCompleted();
    }

   private:
    // Called when a |pipe_reader_| completes reading scan data to a string.
    void OnDataRead(base::Optional<std::string> data) {
      DCHECK(!data_read_);
      data_read_ = true;
      data_ = std::move(data);
      pipe_reader_.reset();
      MaybeCompleted();
    }

    void MaybeCompleted() {
      // If data reading is not yet completed, or D-Bus call does not yet
      // return, wait for the other.
      if (!data_read_ || callback_.is_null())
        return;

      std::move(callback_).Run(std::move(data_));
    }

    std::unique_ptr<chromeos::PipeReader> pipe_reader_;

    // Set to true on data read completion.
    bool data_read_ = false;

    // Available only when |data_read_| is true.
    base::Optional<std::string> data_;

    CompletionCallback callback_;

    base::WeakPtrFactory<ScanDataReader> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(ScanDataReader);
  };

  // Called when ListScanners completes.
  void OnListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ListScannersResponse";
      std::move(callback).Run(base::nullopt);
      return;
    }

    lorgnette::ListScannersResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ListScannersResponse";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Called when a response for ScanImage() is received.
  void OnScanImageComplete(DBusMethodCallback<std::string> callback,
                           std::unique_ptr<ScanDataReader> scan_data_reader,
                           dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to scan image";
      // Do not touch |scan_data_reader|, so that RAII deletes it and
      // cancels the inflight operation.
      std::move(callback).Run(base::nullopt);
      return;
    }
    auto* reader = scan_data_reader.get();
    reader->Wait(
        base::BindOnce(&LorgnetteManagerClientImpl::OnScanDataCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(scan_data_reader)));
  }

  // Called when scan data read is completed.
  // This is to maintain the lifetime of ScanDataReader instance.
  void OnScanDataCompleted(DBusMethodCallback<std::string> callback,
                           std::unique_ptr<ScanDataReader> scan_data_reader,
                           base::Optional<std::string> data) {
    std::move(callback).Run(std::move(data));
  }

  dbus::ObjectProxy* lorgnette_daemon_proxy_ = nullptr;
  base::WeakPtrFactory<LorgnetteManagerClientImpl> weak_ptr_factory_{this};
};

LorgnetteManagerClient::LorgnetteManagerClient() = default;

LorgnetteManagerClient::~LorgnetteManagerClient() = default;

// static
std::unique_ptr<LorgnetteManagerClient> LorgnetteManagerClient::Create() {
  return std::make_unique<LorgnetteManagerClientImpl>();
}

}  // namespace chromeos
