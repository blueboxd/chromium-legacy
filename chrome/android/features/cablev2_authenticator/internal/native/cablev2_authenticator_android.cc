// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/base64url.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

// This "header" is actually contains several function definitions and thus can
// only be included once across Chromium.
#include "chrome/android/features/cablev2_authenticator/internal/jni_headers/BLEHandler_jni.h"

namespace {

// Defragmenter accepts CTAP2 message fragments and reassembles them.
// See
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#ble-framing
class Defragmenter {
 public:
  // Process appends the fragment |in| to the current message. If there is an
  // error, it returns false. Otherwise it returns true and, if a complete
  // message is available, |*out_result| is set to the command value and payload
  // and the Defragmenter is reset for the next message. Otherwise |*out_result|
  // is empty.
  //
  // If this function returns false, the object is no longer usable for future
  // fragments.
  //
  // The span in any |*out_result| value is only valid until the next call on
  // this object and may alias |in|.
  bool Process(base::span<const uint8_t> in,
               base::Optional<std::pair<uint8_t, base::span<const uint8_t>>>*
                   out_result) {
    CBS cbs;
    CBS_init(&cbs, in.data(), in.size());

    uint8_t lead_byte;
    if (!CBS_get_u8(&cbs, &lead_byte)) {
      return false;
    }

    const bool message_start = (lead_byte & 0x80) != 0;
    if (message_start != expect_message_start_) {
      return false;
    }

    if (message_start) {
      // The most-significant bit isn't masked off in order to match up with
      // the values in FidoBleDeviceCommand.
      const uint8_t command = lead_byte;

      uint16_t msg_len;
      if (!CBS_get_u16(&cbs, &msg_len) || msg_len < CBS_len(&cbs)) {
        return false;
      }

      if (msg_len == CBS_len(&cbs)) {
        base::span<const uint8_t> span(CBS_data(&cbs), CBS_len(&cbs));
        out_result->emplace(command, span);
        return true;
      }

      expect_message_start_ = false;
      command_ = command;
      message_len_ = msg_len;
      next_fragment_ = 0;
      buf_.resize(0);
      buf_.insert(buf_.end(), CBS_data(&cbs), CBS_data(&cbs) + CBS_len(&cbs));
      out_result->reset();
      return true;
    }

    if (next_fragment_ != lead_byte) {
      return false;
    }

    buf_.insert(buf_.end(), CBS_data(&cbs), CBS_data(&cbs) + CBS_len(&cbs));

    if (buf_.size() < message_len_) {
      next_fragment_ = (next_fragment_ + 1) & 0x7f;
      out_result->reset();
      return true;
    } else if (buf_.size() > message_len_) {
      return false;
    }

    expect_message_start_ = true;
    out_result->emplace(command_, buf_);
    return true;
  }

 private:
  std::vector<uint8_t> buf_;
  uint8_t command_;
  uint16_t message_len_;
  uint8_t next_fragment_;
  bool expect_message_start_ = true;
};

// AuthenticatorState contains the keys for a caBLE v2 authenticator.
struct AuthenticatorState {
  // TODO: authenticator-global state isn't yet implemented.
  base::Optional<std::pair<std::array<uint8_t, device::kCableNonceSize>,
                           std::array<uint8_t, device::kCableEphemeralIdSize>>>
      pairing_nonce_and_eid;
  base::Optional<std::pair<std::array<uint8_t, device::kCableNonceSize>,
                           std::array<uint8_t, device::kCableEphemeralIdSize>>>
      qr_nonce_and_eid;
  base::Optional<device::CableDiscoveryData> qr_discovery_data;
};

// Client represents the state of a single BLE peer.
class Client {
 public:
  Client(uint16_t mtu, const AuthenticatorState* auth_state)
      : mtu_(mtu), auth_state_(auth_state) {}

  bool Process(
      base::span<const uint8_t> fragment,
      base::Optional<std::vector<std::vector<uint8_t>>>* out_response) {
    if (!ProcessImpl(fragment, out_response)) {
      state_ = State::kError;
      return false;
    }
    return true;
  }

 private:
  enum State {
    kHandshake,
    kConnected,
    kError,
  };

  bool ProcessImpl(
      base::span<const uint8_t> fragment,
      base::Optional<std::vector<std::vector<uint8_t>>>* out_response) {
    out_response->reset();

    if (state_ == State::kError) {
      return false;
    }

    base::Optional<std::pair<uint8_t, base::span<const uint8_t>>> message;
    if (!defrag_.Process(fragment, &message)) {
      FIDO_LOG(ERROR) << "Failed to defragment message";
      return false;
    }

    if (!message) {
      return true;
    }

    std::vector<uint8_t> response;
    switch (state_) {
      case State::kHandshake: {
        if (message->first !=
            static_cast<uint8_t>(device::FidoBleDeviceCommand::kControl)) {
          FIDO_LOG(ERROR) << "Expected control message but received command "
                          << static_cast<unsigned>(message->first);
          return false;
        }

        base::Optional<std::unique_ptr<device::cablev2::Crypter>>
            handshake_result = device::cablev2::RespondToHandshake(
                auth_state_->qr_discovery_data->v2->psk_gen_key,
                auth_state_->qr_nonce_and_eid->first,
                auth_state_->qr_nonce_and_eid->second, /*identity=*/nullptr,
                /*pairing_data=*/nullptr, message->second, &response);
        if (!handshake_result) {
          FIDO_LOG(ERROR) << "Handshake failed";
          return false;
        }
        crypter_ = std::move(handshake_result.value());
        state_ = State::kConnected;
        break;
      }

      case State::kConnected: {
        if (message->first !=
            static_cast<uint8_t>(device::FidoBleDeviceCommand::kMsg)) {
          FIDO_LOG(ERROR) << "Expected normal message but received command "
                          << static_cast<unsigned>(message->first);
          return false;
        }

        std::vector<uint8_t> plaintext;
        if (!crypter_->Decrypt(
                static_cast<device::FidoBleDeviceCommand>(message->first),
                message->second, &plaintext) ||
            plaintext.empty()) {
          FIDO_LOG(ERROR) << "Decryption failed";
          return false;
        }

        base::span<const uint8_t> cbor_bytes = plaintext;
        const auto command = cbor_bytes[0];
        cbor_bytes = cbor_bytes.subspan(1);

        base::Optional<cbor::Value> payload;
        if (!cbor_bytes.empty()) {
          payload = cbor::Reader::Read(cbor_bytes);
          if (!payload) {
            FIDO_LOG(ERROR)
                << "CBOR decoding failed for " << base::HexEncode(cbor_bytes);
            return false;
          }
        }

        switch (command) {
          case static_cast<uint8_t>(
              device::CtapRequestCommand::kAuthenticatorGetInfo): {
            if (payload) {
              FIDO_LOG(ERROR)
                  << "getInfo command incorrectly contained payload";
              return false;
            }

            std::array<uint8_t, device::kAaguidLength> aaguid{};
            device::AuthenticatorGetInfoResponse get_info(
                {device::ProtocolVersion::kCtap2}, aaguid);
            // TODO: should be based on whether a screen-lock is enabled.
            get_info.options.user_verification_availability =
                device::AuthenticatorSupportedOptions::
                    UserVerificationAvailability::kSupportedAndConfigured;
            response =
                device::AuthenticatorGetInfoResponse::EncodeToCBOR(get_info);
            response.insert(response.begin(), 0);
            break;
          }

          case static_cast<uint8_t>(
              device::CtapRequestCommand::kAuthenticatorMakeCredential): {
            if (!payload) {
              return false;
            }

            // TODO: display to GMSCore's WebAuthn API to handle this message.
            return false;
          }

          default:
            FIDO_LOG(ERROR) << "Received unknown command "
                            << static_cast<unsigned>(command);
            return false;
        }

        if (!crypter_->Encrypt(&response)) {
          FIDO_LOG(ERROR) << "Failed to encrypt response";
          return false;
        }

        break;
      }

      case State::kError:
        NOTREACHED();
        return false;
    }

    std::vector<std::vector<uint8_t>> fragments;
    if (!Fragment(message->first, response, &fragments)) {
      FIDO_LOG(ERROR) << "Failed to fragment response of length "
                      << response.size();
      return false;
    }

    out_response->emplace(std::move(fragments));
    return true;
  }

  // Fragment takes a command value and payload and appends one of more
  // fragments to |out_fragments| to respect |mtu_|. It returns true on success
  // and false on error.
  bool Fragment(uint8_t command,
                base::span<const uint8_t> in,
                std::vector<std::vector<uint8_t>>* out_fragments) {
    DCHECK(command & 0x80);

    if (in.size() > 0xffff || mtu_ < 4) {
      return false;
    }
    const size_t max_initial_fragment_bytes = mtu_ - 3;
    const size_t max_subsequent_fragment_bytes = mtu_ - 1;

    std::vector<uint8_t> fragment = {command, (in.size() >> 8) & 0xff,
                                     in.size() & 0xff};
    const size_t todo = std::min(in.size(), max_initial_fragment_bytes);
    fragment.insert(fragment.end(), in.data(), in.data() + todo);
    in = in.subspan(todo);
    out_fragments->emplace_back(std::move(fragment));

    uint8_t frag_num = 0;
    while (!in.empty()) {
      fragment.clear();
      fragment.reserve(mtu_);
      fragment.push_back(frag_num);
      frag_num = (frag_num + 1) & 0x7f;

      const size_t todo = std::min(in.size(), max_subsequent_fragment_bytes);
      fragment.insert(fragment.end(), in.data(), in.data() + todo);
      in = in.subspan(todo);
      out_fragments->emplace_back(std::move(fragment));
    }

    return true;
  }

  const uint16_t mtu_;
  const AuthenticatorState* const auth_state_;
  State state_ = State::kHandshake;
  Defragmenter defrag_;
  std::unique_ptr<device::cablev2::Crypter> crypter_;
};

// CableInterface is a singleton that receives events from BLEHandler.java:
// the code that interfaces to Android's BLE stack. All calls into this
// object happen on a single thread.
class CableInterface {
 public:
  static CableInterface* GetInstance() {
    return base::Singleton<CableInterface>::get();
  }

  void Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& ble_handler) {
    ble_handler_.Reset(ble_handler);
    env_ = env;
  }

  void Stop() {
    ble_handler_.Reset();
    clients_.clear();
    known_mtus_.clear();
    auth_state_.qr_nonce_and_eid.reset();
    auth_state_.pairing_nonce_and_eid.reset();
    auth_state_.qr_discovery_data.reset();
    env_ = nullptr;
  }

  void OnQRScanned(const std::string& qr_url) {
    static const char kPrefix[] = "fido://c1/";
    DCHECK(qr_url.find(kPrefix) == 0);

    base::StringPiece qr_url_base64(qr_url);
    qr_url_base64 = qr_url_base64.substr(sizeof(kPrefix) - 1);
    std::string qr_secret_str;
    if (!base::Base64UrlDecode(qr_url_base64,
                               base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                               &qr_secret_str) ||
        qr_secret_str.size() != device::kCableQRSecretSize) {
      FIDO_LOG(ERROR) << "QR decoding failed: " << qr_url;
      return;
    }

    uint8_t qr_secret[device::kCableQRSecretSize];
    memcpy(qr_secret, qr_secret_str.data(), sizeof(qr_secret));
    auth_state_.qr_discovery_data.emplace(qr_secret);

    std::array<uint8_t, device::kCableNonceSize> nonce;
    RAND_bytes(nonce.data(), nonce.size());

    uint8_t eid_plaintext[AES_BLOCK_SIZE];
    static_assert(sizeof(eid_plaintext) == AES_BLOCK_SIZE,
                  "EIDs are not AES blocks");
    AES_KEY key;
    CHECK(
        AES_set_encrypt_key(
            auth_state_.qr_discovery_data->v2->eid_gen_key.data(),
            /*bits=*/8 * auth_state_.qr_discovery_data->v2->eid_gen_key.size(),
            &key) == 0);
    memcpy(eid_plaintext, nonce.data(), nonce.size());
    memset(eid_plaintext + nonce.size(), 0,
           sizeof(eid_plaintext) - nonce.size());

    std::array<uint8_t, AES_BLOCK_SIZE> eid;
    AES_encrypt(/*in=*/eid_plaintext, /*out=*/eid.data(), &key);

    auth_state_.qr_nonce_and_eid.emplace(nonce, eid);

    base::android::ScopedJavaLocalRef<jbyteArray> jbytes(
        env_, env_->NewByteArray(sizeof(eid)));
    env_->SetByteArrayRegion(jbytes.obj(), 0, eid.size(), (jbyte*)eid.data());
    Java_BLEHandler_sendBLEAdvert(env_, ble_handler_, jbytes);
  }

  void RecordClientMTU(uint64_t client_adr, uint16_t mtu_bytes) {
    known_mtus_.emplace(client_adr, mtu_bytes);
  }

  base::android::ScopedJavaLocalRef<jobjectArray> Write(
      jlong client_addr,
      const base::android::JavaParamRef<jbyteArray>& data) {
    auto it = clients_.find(client_addr);
    if (it == clients_.end()) {
      DCHECK(known_mtus_.find(client_addr) != known_mtus_.end());
      uint16_t mtu = known_mtus_[client_addr];
      if (mtu == 0) {
        mtu = 512;
      }
      it = clients_.emplace(client_addr, new Client(mtu, &auth_state_)).first;
    }
    Client* const client = it->second.get();

    size_t data_len = env_->GetArrayLength(data);
    jbyte* data_bytes = env_->GetByteArrayElements(data, nullptr);

    base::Optional<std::vector<std::vector<uint8_t>>> response_fragments;
    if (!client->Process(base::span<const uint8_t>(
                             reinterpret_cast<uint8_t*>(data_bytes), data_len),
                         &response_fragments)) {
      return nullptr;
    }

    base::android::ScopedJavaLocalRef<jclass> byte_array_class(
        env_, env_->FindClass("[B"));
    if (!response_fragments) {
      base::android::ScopedJavaLocalRef<jobjectArray> ret(
          env_, env_->NewObjectArray(0, byte_array_class.obj(), nullptr));
      return ret;
    }

    base::android::ScopedJavaLocalRef<jobjectArray> ret(
        env_, env_->NewObjectArray(response_fragments->size(),
                                   byte_array_class.obj(), nullptr));
    for (size_t i = 0; i < response_fragments->size(); i++) {
      const std::vector<uint8_t>& fragment = response_fragments->at(i);

      base::android::ScopedJavaLocalRef<jbyteArray> jbytes(
          env_, env_->NewByteArray(fragment.size()));
      env_->SetByteArrayRegion(jbytes.obj(), 0, fragment.size(),
                               reinterpret_cast<const jbyte*>(fragment.data()));
      env_->SetObjectArrayElement(ret.obj(), i, jbytes.obj());
    }

    return ret;
  }

 private:
  friend struct base::DefaultSingletonTraits<CableInterface>;
  CableInterface() = default;

  JNIEnv* env_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> ble_handler_;
  AuthenticatorState auth_state_;
  std::map<uint64_t, uint16_t> known_mtus_;
  std::map<uint64_t, std::unique_ptr<Client>> clients_;
};

}  // anonymous namespace

// These functions are the entry points for BLEHandler.java calling into C++.

static void JNI_BLEHandler_Start(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& ble_handler) {
  CableInterface::GetInstance()->Start(env, ble_handler);
}

static void JNI_BLEHandler_Stop(JNIEnv* env) {
  CableInterface::GetInstance()->Stop();
}

static void JNI_BLEHandler_OnQRScanned(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jvalue) {
  CableInterface::GetInstance()->OnQRScanned(
      base::android::ConvertJavaStringToUTF8(jvalue));
}

static void JNI_BLEHandler_RecordClientMtu(JNIEnv* env,
                                           jlong client,
                                           jint mtu_bytes) {
  if (mtu_bytes > 0xffff) {
    mtu_bytes = 0xffff;
  }
  CableInterface::GetInstance()->RecordClientMTU(client, mtu_bytes);
}

static base::android::ScopedJavaLocalRef<jobjectArray> JNI_BLEHandler_Write(
    JNIEnv* env,
    jlong client,
    const base::android::JavaParamRef<jbyteArray>& data) {
  return CableInterface::GetInstance()->Write(client, data);
}
