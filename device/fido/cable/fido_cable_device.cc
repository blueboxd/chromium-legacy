// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_device.h"

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/ble/fido_ble_connection.h"
#include "device/fido/ble/fido_ble_frames.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

// Maximum size of EncryptionData::read_sequence_num or
// EncryptionData::write_sequence_num allowed. If we encounter
// counter larger than |kMaxCounter| FidoCableDevice should error out.
constexpr uint32_t kMaxCounter = (1 << 24) - 1;

base::Optional<std::vector<uint8_t>> ConstructV1Nonce(
    base::span<const uint8_t> nonce,
    bool is_sender_client,
    uint32_t counter) {
  if (counter > kMaxCounter)
    return base::nullopt;

  auto constructed_nonce = fido_parsing_utils::Materialize(nonce);
  constructed_nonce.push_back(is_sender_client ? 0x00 : 0x01);
  constructed_nonce.push_back(counter >> 16 & 0xFF);
  constructed_nonce.push_back(counter >> 8 & 0xFF);
  constructed_nonce.push_back(counter & 0xFF);
  return constructed_nonce;
}

}  // namespace

FidoCableDevice::EncryptionData::EncryptionData() = default;
FidoCableDevice::EncryptionData::~EncryptionData() = default;

FidoCableDevice::FidoCableDevice(BluetoothAdapter* adapter, std::string address)
    : FidoBleDevice(adapter, std::move(address), FidoBleDevice::Type::kCaBLE) {}

FidoCableDevice::FidoCableDevice(std::unique_ptr<FidoBleConnection> connection)
    : FidoBleDevice(std::move(connection)) {}

FidoCableDevice::~FidoCableDevice() = default;

FidoDevice::CancelToken FidoCableDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  if ((!encryption_data_ && !v2_crypter_) ||
      !EncryptOutgoingMessage(&command)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    state_ = State::kDeviceError;
    FIDO_LOG(ERROR) << "Failed to encrypt outgoing caBLE message.";
    return 0;
  }

  FIDO_LOG(DEBUG) << "Sending encrypted message to caBLE client";
  return AddToPendingFrames(FidoBleDeviceCommand::kMsg, std::move(command),
                            std::move(callback));
}

void FidoCableDevice::OnResponseFrame(FrameCallback callback,
                                      base::Optional<FidoBleFrame> frame) {
  // The request is done, time to reset |transaction_|.
  ResetTransaction();
  state_ = frame ? State::kReady : State::kDeviceError;

  if (frame && frame->command() != FidoBleDeviceCommand::kControl) {
    if ((!encryption_data_ && !v2_crypter_) ||
        !DecryptIncomingMessage(&frame.value())) {
      state_ = State::kDeviceError;
      frame = base::nullopt;
    }
  }

  auto self = GetWeakPtr();
  std::move(callback).Run(std::move(frame));

  // Executing callbacks may free |this|. Check |self| first.
  if (self)
    Transition();
}

base::WeakPtr<FidoDevice> FidoCableDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoCableDevice::SendHandshakeMessage(
    std::vector<uint8_t> handshake_message,
    DeviceCallback callback) {
  AddToPendingFrames(FidoBleDeviceCommand::kControl,
                     std::move(handshake_message), std::move(callback));
}

void FidoCableDevice::SetV1EncryptionData(
    base::span<const uint8_t, 32> session_key,
    base::span<const uint8_t, 8> nonce) {
  // Encryption data must be set at most once during Cable handshake protocol.
  DCHECK(!encryption_data_);
  DCHECK(!v2_crypter_);
  encryption_data_.emplace();
  encryption_data_->read_key = fido_parsing_utils::Materialize(session_key);
  encryption_data_->write_key = fido_parsing_utils::Materialize(session_key);
  encryption_data_->nonce = fido_parsing_utils::Materialize(nonce);
}

void FidoCableDevice::SetV2EncryptionData(
    std::unique_ptr<cablev2::Crypter> crypter) {
  DCHECK(!encryption_data_);
  DCHECK(!v2_crypter_);
  v2_crypter_.emplace(std::move(crypter));
}

FidoTransportProtocol FidoCableDevice::DeviceTransport() const {
  return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
}

void FidoCableDevice::SetSequenceNumbersForTesting(uint32_t read_seq,
                                                   uint32_t write_seq) {
  encryption_data_->write_sequence_num = write_seq;
  encryption_data_->read_sequence_num = read_seq;
}

bool FidoCableDevice::EncryptOutgoingMessage(
    std::vector<uint8_t>* message_to_encrypt) {
  if (v2_crypter_) {
    return v2_crypter_.value()->Encrypt(message_to_encrypt);
  }

  return EncryptV1OutgoingMessage(&encryption_data_.value(),
                                  message_to_encrypt);
}

bool FidoCableDevice::DecryptIncomingMessage(FidoBleFrame* incoming_frame) {
  if (v2_crypter_) {
    std::vector<uint8_t> plaintext;
    if (!v2_crypter_.value()->Decrypt(incoming_frame->command(),
                                      incoming_frame->data(), &plaintext)) {
      return false;
    }
    incoming_frame->data().swap(plaintext);
    return true;
  }

  return DecryptV1IncomingMessage(&encryption_data_.value(), incoming_frame);
}

// static
bool FidoCableDevice::EncryptV1OutgoingMessage(
    EncryptionData* encryption_data,
    std::vector<uint8_t>* message_to_encrypt) {
  const auto nonce =
      ConstructV1Nonce(encryption_data->nonce, /*is_sender_client=*/true,
                       encryption_data->write_sequence_num++);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data->write_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(*message_to_encrypt, *nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

// static
bool FidoCableDevice::DecryptV1IncomingMessage(EncryptionData* encryption_data,
                                               FidoBleFrame* incoming_frame) {
  const auto nonce =
      ConstructV1Nonce(encryption_data->nonce, /*is_sender_client=*/false,
                       encryption_data->read_sequence_num);
  if (!nonce)
    return false;

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(encryption_data->read_key);
  DCHECK_EQ(nonce->size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {
      base::strict_cast<uint8_t>(incoming_frame->command())};
  base::Optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(incoming_frame->data(), *nonce, additional_data);
  if (!plaintext) {
    FIDO_LOG(ERROR) << "Failed to decrypt caBLE message.";
    return false;
  }

  encryption_data->read_sequence_num++;
  incoming_frame->data().swap(*plaintext);
  return true;
}

}  // namespace device
