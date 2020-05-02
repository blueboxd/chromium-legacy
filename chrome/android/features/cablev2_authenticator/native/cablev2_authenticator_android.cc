// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64url.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

// These "headers" actually contain several function definitions and thus can
// only be included once across Chromium.
#include "chrome/android/features/cablev2_authenticator/jni_headers/BLEHandler_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

using device::CtapDeviceResponseCode;
using device::CtapRequestCommand;
using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::Map;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;
using device::cbor_extract::StringKey;
using device::fido_parsing_utils::CopyCBORBytestring;

namespace {

// TODO: this string is currently in the protocol, and saved in the
// desktop's prefs, but not otherwise surfaced. See if we can get a better
// value for it.
constexpr char kDeviceName[] = "Android phone";

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
  // pairing_data contains long-term keys, and information that is potentially
  // sent to peers during QR pairing. The |v2| member of this structure will be
  // populated.
  device::CableDiscoveryData pairing_data;
  // identity_key is the long-term signing key.
  bssl::UniquePtr<EC_KEY> identity_key;

  // pairing_advert contains information about the BLE advert that is sent based
  // on the long-term keys.
  device::cablev2::NonceAndEID pairing_advert;

  // If doing a QR pairing, the following two members will be present.

  // qr_advert contains information about the BLE advert that is sent based on
  // QR pairing keys.
  base::Optional<device::cablev2::NonceAndEID> qr_advert;
  // qr_psk_gen_key contains the PSK generating key derived from the QR secret.
  base::Optional<device::CablePskGeneratorKey> qr_psk_gen_key;
  // peer_identity is the public-key of the desktop from the scanned QR code.
  base::Optional<bssl::UniquePtr<EC_POINT>> qr_peer_identity;
};

struct MakeCredRequest {
  const std::vector<uint8_t>* client_data_hash;
  const std::string* rp_id;
  const std::vector<uint8_t>* user_id;
  const cbor::Value::ArrayValue* cred_params;
  const cbor::Value::ArrayValue* excluded_credentials;
  const std::string* origin;
  const std::vector<uint8_t>* challenge;
};

static constexpr StepOrByte<MakeCredRequest> kMakeCredParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, MakeCredRequest, client_data_hash),
    IntKey<MakeCredRequest>(1),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(2),
      ELEMENT(Is::kRequired, MakeCredRequest, rp_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',
    Stop<MakeCredRequest>(),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(3),
      ELEMENT(Is::kRequired, MakeCredRequest, user_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',
    Stop<MakeCredRequest>(),

    ELEMENT(Is::kRequired, MakeCredRequest, cred_params),
    IntKey<MakeCredRequest>(4),
    ELEMENT(Is::kOptional, MakeCredRequest, excluded_credentials),
    IntKey<MakeCredRequest>(5),

    // TODO: remove once the FIDO API can handle clientDataJSON
    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(6),
      Map<MakeCredRequest>(),
      StringKey<MakeCredRequest>(),
          'g', 'o', 'o', 'g', 'l', 'e', 'A', 'n', 'd', 'r', 'o', 'i', 'd',
          'C', 'l', 'i', 'e', 'n', 't', 'D', 'a', 't', 'a', '\0',
          ELEMENT(Is::kRequired, MakeCredRequest, origin),
          IntKey<MakeCredRequest>(2),

          ELEMENT(Is::kRequired, MakeCredRequest, challenge),
          IntKey<MakeCredRequest>(3),
      Stop<MakeCredRequest>(),
    Stop<MakeCredRequest>(),

    Stop<MakeCredRequest>(),
    // clang-format on
};

struct AttestationObject {
  const std::string* fmt;
  const std::vector<uint8_t>* auth_data;
  const cbor::Value* statement;
};

static constexpr StepOrByte<AttestationObject> kAttObjParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, AttestationObject, fmt),
    StringKey<AttestationObject>(), 'f', 'm', 't', '\0',

    ELEMENT(Is::kRequired, AttestationObject, auth_data),
    StringKey<AttestationObject>(), 'a', 'u', 't', 'h', 'D', 'a', 't', 'a', '\0',

    ELEMENT(Is::kRequired, AttestationObject, statement),
    StringKey<AttestationObject>(), 'a', 't', 't', 'S', 't', 'm', 't', '\0',
    Stop<AttestationObject>(),
    // clang-format on
};

struct GetAssertionRequest {
  const std::string* rp_id;
  const std::vector<uint8_t>* client_data_hash;
  const cbor::Value::ArrayValue* allowed_credentials;
  const std::string* origin;
  const std::vector<uint8_t>* challenge;
};

static constexpr StepOrByte<GetAssertionRequest> kGetAssertionParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, GetAssertionRequest, rp_id),
    IntKey<GetAssertionRequest>(1),

    ELEMENT(Is::kRequired, GetAssertionRequest, client_data_hash),
    IntKey<GetAssertionRequest>(2),

    ELEMENT(Is::kOptional, GetAssertionRequest, allowed_credentials),
    IntKey<GetAssertionRequest>(3),

    // TODO: remove once the FIDO API can handle clientDataJSON
    Map<GetAssertionRequest>(),
    IntKey<GetAssertionRequest>(4),
      Map<GetAssertionRequest>(),
      StringKey<GetAssertionRequest>(),
          'g', 'o', 'o', 'g', 'l', 'e', 'A', 'n', 'd', 'r', 'o', 'i', 'd',
          'C', 'l', 'i', 'e', 'n', 't', 'D', 'a', 't', 'a', '\0',
          ELEMENT(Is::kRequired, GetAssertionRequest, origin),
          IntKey<GetAssertionRequest>(2),

          ELEMENT(Is::kRequired, GetAssertionRequest, challenge),
          IntKey<GetAssertionRequest>(3),
      Stop<GetAssertionRequest>(),
    Stop<GetAssertionRequest>(),

    Stop<GetAssertionRequest>(),
    // clang-format on
};

// Client represents the state of a single BLE peer.
class Client {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void MakeCredential(
        Client* client,
        const std::string& origin,
        const std::string& rp_id,
        base::span<const uint8_t> challenge,
        base::span<const uint8_t> user_id,
        base::span<const int> algorithms,
        base::span<std::vector<uint8_t>> excluded_credential_ids,
        bool resident_key_required) = 0;
    virtual void GetAssertion(
        Client* client,
        const std::string& origin,
        const std::string& rp_id,
        base::span<const uint8_t> challenge,
        base::span<std::vector<uint8_t>> allowed_credential_ids) = 0;
  };

  Client(uint64_t addr,
         uint16_t mtu,
         const AuthenticatorState* auth_state,
         Delegate* delegate)
      : addr_(addr), mtu_(mtu), auth_state_(auth_state), delegate_(delegate) {}

  bool Process(
      base::span<const uint8_t> fragment,
      base::Optional<std::vector<std::vector<uint8_t>>>* out_response) {
    if (!ProcessImpl(fragment, out_response)) {
      state_ = State::kError;
      return false;
    }
    return true;
  }

  base::Optional<std::vector<std::vector<uint8_t>>> EncryptAndFragment(
      base::span<uint8_t> data) {
    std::vector<uint8_t> encrypted_data =
        device::fido_parsing_utils::Materialize(data);
    if (!crypter_->Encrypt(&encrypted_data)) {
      FIDO_LOG(ERROR) << "Failed to encrypt response";
      return base::nullopt;
    }
    std::vector<std::vector<uint8_t>> fragments;
    if (!Fragment(static_cast<uint8_t>(device::FidoBleDeviceCommand::kMsg),
                  encrypted_data, &fragments)) {
      FIDO_LOG(ERROR) << "Failed to fragment response of length "
                      << encrypted_data.size();
      return base::nullopt;
    }
    return fragments;
  }

  uint64_t addr() { return addr_; }
  uint16_t mtu() { return mtu_; }

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

        // The handshake is prefixed with the EID that the peer is responding
        // to. This allows us to handle the case where we have started
        // advertising for a QR code, but the desktop is already paired and is
        // connecting based on long-term keys.
        device::CableEidArray requested_eid;
        if (!device::fido_parsing_utils::ExtractArray(message->second, 0,
                                                      &requested_eid)) {
          return false;
        }

        base::Optional<std::unique_ptr<device::cablev2::Crypter>>
            handshake_result;
        if (requested_eid == auth_state_->pairing_advert.second) {
          handshake_result = device::cablev2::RespondToHandshake(
              auth_state_->pairing_data.v2->psk_gen_key,
              auth_state_->pairing_advert, auth_state_->identity_key.get(),
              /*peer_identity=*/nullptr, /*pairing_data=*/nullptr,
              message->second, &response);
        } else if (auth_state_->qr_advert.has_value() &&
                   requested_eid == auth_state_->qr_advert->second) {
          // TODO: QR handshakes currently always send pairing data, but it's
          // optional in the protocol.
          handshake_result = device::cablev2::RespondToHandshake(
              *auth_state_->qr_psk_gen_key, *auth_state_->qr_advert,
              /*identity=*/nullptr, auth_state_->qr_peer_identity->get(),
              &auth_state_->pairing_data, message->second, &response);
        } else {
          FIDO_LOG(ERROR) << "Peer is connecting to unknown EID "
                          << base::HexEncode(requested_eid);
          return false;
        }

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
          FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1) << ") "
                          << cbor::DiagnosticWriter::Write(*payload);
        } else {
          FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1)
                          << ") <no payload>";
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
            std::vector<cbor::Value> versions;
            versions.emplace_back("FIDO_2_0");
            std::vector<cbor::Value> extensions;
            extensions.emplace_back(device::kExtensionAndroidClientData);
            // TODO: should be based on whether a screen-lock is enabled.
            cbor::Value::MapValue options;
            options.emplace("uv", true);

            cbor::Value::MapValue response_map;
            response_map.emplace(1, std::move(versions));
            response_map.emplace(2, std::move(extensions));
            response_map.emplace(3, aaguid);
            response_map.emplace(4, std::move(options));

            base::Optional<std::vector<uint8_t>> response_bytes(
                cbor::Writer::Write(cbor::Value(std::move(response_map))));
            response = std::move(*response_bytes);
            response.insert(response.begin(), 0);
            break;
          }

          case static_cast<uint8_t>(
              device::CtapRequestCommand::kAuthenticatorMakeCredential): {
            if (!payload || !payload->is_map()) {
              FIDO_LOG(ERROR) << "Invalid makeCredential payload";
              return false;
            }

            MakeCredRequest make_cred_request;
            if (!device::cbor_extract::Extract<MakeCredRequest>(
                    &make_cred_request, kMakeCredParseSteps,
                    payload->GetMap())) {
              LOG(ERROR) << "Failed to parse makeCredential request";
              return false;
            }

            std::vector<int> algorithms;
            if (!device::cbor_extract::ForEachPublicKeyEntry(
                    *make_cred_request.cred_params, cbor::Value("alg"),
                    base::BindRepeating(
                        [](std::vector<int>* out,
                           const cbor::Value& value) -> bool {
                          if (!value.is_integer()) {
                            return false;
                          }
                          const int64_t alg = value.GetInteger();

                          if (alg > std::numeric_limits<int>::max() ||
                              alg < std::numeric_limits<int>::min()) {
                            return false;
                          }
                          out->push_back(static_cast<int>(alg));
                          return true;
                        },
                        base::Unretained(&algorithms)))) {
              return false;
            }

            std::vector<std::vector<uint8_t>> excluded_credential_ids;
            if (make_cred_request.excluded_credentials &&
                !device::cbor_extract::ForEachPublicKeyEntry(
                    *make_cred_request.excluded_credentials, cbor::Value("id"),
                    base::BindRepeating(
                        [](std::vector<std::vector<uint8_t>>* out,
                           const cbor::Value& value) -> bool {
                          if (!value.is_bytestring()) {
                            return false;
                          }
                          out->push_back(value.GetBytestring());
                          return true;
                        },
                        base::Unretained(&excluded_credential_ids)))) {
              return false;
            }

            // TODO: plumb the rk flag through once GmsCore supports resident
            // keys. This will require support for optional maps in |Extract|.
            delegate_->MakeCredential(
                this, *make_cred_request.origin, *make_cred_request.rp_id,
                *make_cred_request.challenge, *make_cred_request.user_id,
                algorithms, excluded_credential_ids,
                /*resident_key=*/false);
            return true;
          }

          case static_cast<uint8_t>(
              device::CtapRequestCommand::kAuthenticatorGetAssertion): {
            if (!payload || !payload->is_map()) {
              FIDO_LOG(ERROR) << "Invalid makeCredential payload";
              return false;
            }
            GetAssertionRequest get_assertion_request;
            if (!device::cbor_extract::Extract<GetAssertionRequest>(
                    &get_assertion_request, kGetAssertionParseSteps,
                    payload->GetMap())) {
              FIDO_LOG(ERROR) << "Failed to parse getAssertion request";
              return false;
            }

            std::vector<std::vector<uint8_t>> allowed_credential_ids;
            if (get_assertion_request.allowed_credentials &&
                !device::cbor_extract::ForEachPublicKeyEntry(
                    *get_assertion_request.allowed_credentials,
                    cbor::Value("id"),
                    base::BindRepeating(
                        [](std::vector<std::vector<uint8_t>>* out,
                           const cbor::Value& value) -> bool {
                          if (!value.is_bytestring()) {
                            return false;
                          }
                          out->push_back(value.GetBytestring());
                          return true;
                        },
                        base::Unretained(&allowed_credential_ids)))) {
              return false;
            }

            delegate_->GetAssertion(this, *get_assertion_request.origin,
                                    *get_assertion_request.rp_id,
                                    *get_assertion_request.challenge,
                                    allowed_credential_ids);
            return true;
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

  const uint64_t addr_;
  const uint16_t mtu_;
  const AuthenticatorState* const auth_state_;
  State state_ = State::kHandshake;
  Defragmenter defrag_;
  std::unique_ptr<device::cablev2::Crypter> crypter_;
  Delegate* delegate_;
};

// CableInterface is a singleton that receives events from BLEHandler.java:
// the code that interfaces to Android's BLE stack. All calls into this
// object happen on a single thread.
class CableInterface : public Client::Delegate {
 public:
  static CableInterface* GetInstance() {
    return base::Singleton<CableInterface>::get();
  }

  void Start(JNIEnv* env,
             const JavaParamRef<jobject>& ble_handler,
             const JavaParamRef<jbyteArray>& state_bytes) {
    ble_handler_.Reset(ble_handler);
    env_ = env;

    if (!ParseState(state_bytes)) {
      FIDO_LOG(ERROR) << "ParseState() failed";
      GenerateFreshStateAndStore();
    }

    // At this point, the version two pairing data has been established, either
    // because it was parsed from the state, or because it was freshly generated
    // and saved.
    DCHECK(auth_state_.pairing_data.v2.has_value());
    DCHECK(auth_state_.identity_key);

    StartAdvertising(auth_state_.pairing_data.v2->eid_gen_key,
                     &auth_state_.pairing_advert);
  }

  void Stop() {
    ble_handler_.Reset();
    auth_state_.identity_key.reset();
    auth_state_.qr_advert.reset();
    auth_state_.qr_psk_gen_key.reset();
    client_ = nullptr;
    env_ = nullptr;
  }

  void OnQRScanned(const std::string& qr_url) {
    static const char kPrefix[] = "fido://c1/";
    DCHECK(qr_url.find(kPrefix) == 0);

    base::StringPiece qr_url_base64(qr_url);
    qr_url_base64 = qr_url_base64.substr(sizeof(kPrefix) - 1);
    std::string qr_data_str;
    if (!base::Base64UrlDecode(qr_url_base64,
                               base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                               &qr_data_str) ||
        qr_data_str.size() != device::kCableQRDataSize) {
      FIDO_LOG(ERROR) << "QR decoding failed: " << qr_url;
      return;
    }

    const base::Optional<device::CableDiscoveryData> discovery_data =
        device::CableDiscoveryData::FromQRData(
            base::span<const uint8_t, device::kCableQRDataSize>(
                reinterpret_cast<const uint8_t*>(qr_data_str.data()),
                qr_data_str.size()));
    if (!discovery_data) {
      FIDO_LOG(ERROR) << "Failed to decode QR data from: " << qr_url;
      return;
    }

    auth_state_.qr_psk_gen_key.emplace(discovery_data->v2->psk_gen_key);

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    auth_state_.qr_peer_identity.emplace(EC_POINT_new(p256.get()));
    CHECK(EC_POINT_oct2point(p256.get(), auth_state_.qr_peer_identity->get(),
                             discovery_data->v2->peer_identity->data(),
                             discovery_data->v2->peer_identity->size(),
                             /*ctx=*/nullptr));

    StartAdvertising(discovery_data->v2->eid_gen_key,
                     &auth_state_.qr_advert.emplace());
  }

  ScopedJavaLocalRef<jobjectArray> Write(uint64_t client_addr,
                                         uint16_t mtu,
                                         const JavaParamRef<jbyteArray>& data) {
    // First client to write to the fidoControlPoint characteristic becomes the
    // only permissible client for the lifetime of this instance. The Java side
    // filters writes from all other clients.
    if (client_ == nullptr) {
      client_ = std::make_unique<Client>(client_addr, mtu, &auth_state_, this);
    } else if (client_->addr() != static_cast<uint64_t>(client_addr)) {
      NOTREACHED() << "Write from unknown client " << client_->addr();
      return nullptr;
    }
    if (client_->mtu() != mtu) {
      // MTU must not be changed after the initial write. Keep going, but things
      // might fail at this point.
      FIDO_LOG(ERROR) << "MTU changed after first write: " << client_->mtu()
                      << " != " << mtu;
    }

    const size_t data_len = env_->GetArrayLength(data);
    jbyte* data_bytes = env_->GetByteArrayElements(data, /*iscopy=*/nullptr);
    base::Optional<std::vector<std::vector<uint8_t>>> response_fragments;
    const bool process_ok =
        client_->Process(base::span<const uint8_t>(
                             reinterpret_cast<uint8_t*>(data_bytes), data_len),
                         &response_fragments);
    env_->ReleaseByteArrayElements(data, data_bytes, JNI_ABORT);
    if (!process_ok) {
      return nullptr;
    }

    static std::vector<std::vector<uint8_t>> kEmptyFragments;
    return ToJavaArrayOfByteArray(
        env_, response_fragments ? *response_fragments : kEmptyFragments);
  }

  void MakeCredential(Client* client,
                      const std::string& origin,
                      const std::string& rp_id,
                      base::span<const uint8_t> challenge,
                      base::span<const uint8_t> user_id,
                      base::span<const int> algorithms,
                      base::span<std::vector<uint8_t>> excluded_credential_ids,
                      bool resident_key_required) override {
    Java_BLEHandler_makeCredential(
        env_, ble_handler_, client->addr(),
        ConvertUTF8ToJavaString(env_, origin),
        ConvertUTF8ToJavaString(env_, rp_id), ToJavaByteArray(env_, challenge),
        // TODO: Pass full user entity once resident key support is added.
        ToJavaByteArray(env_, user_id), ToJavaIntArray(env_, algorithms),
        ToJavaArrayOfByteArray(env_, excluded_credential_ids),
        resident_key_required);
  }

  void GetAssertion(
      Client* client,
      const std::string& origin,
      const std::string& rp_id,
      base::span<const uint8_t> challenge,
      base::span<std::vector<uint8_t>> allowed_credential_ids) override {
    Java_BLEHandler_getAssertion(
        env_, ble_handler_, client->addr(),
        ConvertUTF8ToJavaString(env_, origin),
        ConvertUTF8ToJavaString(env_, rp_id), ToJavaByteArray(env_, challenge),
        ToJavaArrayOfByteArray(env_, allowed_credential_ids));
  }

  void OnMakeCredentialResponse(uint64_t client_addr,
                                uint32_t ctap_status,
                                base::span<const uint8_t> client_data_json,
                                base::span<const uint8_t> attestation_object) {
    DCHECK_LE(ctap_status, 0xFFu);
    if (!client_) {
      NOTREACHED() << "OnMakeCredentialResponse() without a connected client";
      return;
    }
    DCHECK_EQ(client_->addr(), client_addr);

    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      // TODO: pass response parameters from the Java side.
      base::Optional<cbor::Value> cbor_attestation_object =
          cbor::Reader::Read(attestation_object);
      if (!cbor_attestation_object || !cbor_attestation_object->is_map()) {
        FIDO_LOG(ERROR) << "invalid CBOR attestation object";
        return;
      }

      AttestationObject attestation_object;
      if (!device::cbor_extract::Extract<AttestationObject>(
              &attestation_object, kAttObjParseSteps,
              cbor_attestation_object->GetMap())) {
        FIDO_LOG(ERROR) << "attestation object parse failed";
        return;
      }

      cbor::Value::MapValue response_map;
      response_map.emplace(1, base::StringPiece(*attestation_object.fmt));
      response_map.emplace(
          2, base::span<const uint8_t>(*attestation_object.auth_data));
      response_map.emplace(3, attestation_object.statement->Clone());
      response_map.emplace(device::kAndroidClientDataExtOutputKey,
                           client_data_json);

      base::Optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    }

    base::Optional<std::vector<std::vector<uint8_t>>> response_fragments =
        client_->EncryptAndFragment(response);
    if (!response_fragments) {
      FIDO_LOG(ERROR) << "EncryptAndFragment() failed for " << client_addr;
      return;
    }

    Java_BLEHandler_sendNotification(
        env_, ble_handler_, client_addr,
        ToJavaArrayOfByteArray(env_, *response_fragments));
  }

  void OnGetAssertionResponse(uint64_t client_addr,
                              uint32_t ctap_status,
                              std::vector<uint8_t> client_data_json,
                              std::vector<uint8_t> credential_id,
                              std::vector<uint8_t> authenticator_data,
                              std::vector<uint8_t> signature) {
    DCHECK_LE(ctap_status, 0xFFu);
    if (!client_) {
      NOTREACHED() << "OnGetAssertionResponse() without a connected client";
      return;
    }
    DCHECK_EQ(client_->addr(), client_addr);

    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};

    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      cbor::Value::MapValue credential_descriptor;
      credential_descriptor.emplace("type", device::kPublicKey);
      credential_descriptor.emplace("id", credential_id);
      cbor::Value::ArrayValue transports;
      transports.emplace_back("internal");
      transports.emplace_back("cable");
      credential_descriptor.emplace("transports", std::move(transports));
      cbor::Value::MapValue response_map;
      response_map.emplace(1, std::move(credential_descriptor));
      response_map.emplace(2, authenticator_data);
      response_map.emplace(3, signature);
      // TODO: add user entity to support resident keys.
      response_map.emplace(device::kAndroidClientDataExtOutputKey,
                           client_data_json);

      base::Optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    }

    base::Optional<std::vector<std::vector<uint8_t>>> response_fragments =
        client_->EncryptAndFragment(response);
    if (!response_fragments) {
      FIDO_LOG(ERROR) << "EncryptAndFragment() failed for " << client_addr;
      return;
    }

    Java_BLEHandler_sendNotification(
        env_, ble_handler_, client_addr,
        ToJavaArrayOfByteArray(env_, *response_fragments));
  }

 private:
  friend struct base::DefaultSingletonTraits<CableInterface>;
  CableInterface() = default;

  void StartAdvertising(const device::CableEidGeneratorKey& eid_gen_key,
                        device::cablev2::NonceAndEID* out_nonce_and_eid) {
    std::array<uint8_t, device::kCableNonceSize> nonce;
    crypto::RandBytes(nonce);

    uint8_t eid_plaintext[device::kCableEphemeralIdSize];
    static_assert(sizeof(eid_plaintext) == AES_BLOCK_SIZE,
                  "EIDs are not AES blocks");
    AES_KEY key;
    CHECK(AES_set_encrypt_key(eid_gen_key.data(),
                              /*bits=*/8 * eid_gen_key.size(), &key) == 0);
    memcpy(eid_plaintext, nonce.data(), nonce.size());
    static_assert(sizeof(nonce) < sizeof(eid_plaintext), "Nonces too large");
    memset(eid_plaintext + nonce.size(), 0,
           sizeof(eid_plaintext) - nonce.size());

    std::array<uint8_t, AES_BLOCK_SIZE> eid;
    AES_encrypt(/*in=*/eid_plaintext, /*out=*/eid.data(), &key);

    out_nonce_and_eid->first = nonce;
    out_nonce_and_eid->second = eid;

    Java_BLEHandler_sendBLEAdvert(env_, ble_handler_,
                                  ToJavaByteArray(env_, eid));
  }

  bool ParseState(const JavaParamRef<jbyteArray>& state_bytes) {
    if (!state_bytes) {
      return false;
    }

    base::span<const uint8_t> state_bytes_span(
        reinterpret_cast<uint8_t*>(
            env_->GetByteArrayElements(state_bytes, nullptr)),
        env_->GetArrayLength(state_bytes));
    base::Optional<cbor::Value> state = cbor::Reader::Read(state_bytes_span);
    if (!state || !state->is_map()) {
      return false;
    }

    const cbor::Value::MapValue& state_map(state->GetMap());
    device::CableDiscoveryData::V2Data& pairing_data =
        auth_state_.pairing_data.v2.emplace();
    std::array<uint8_t, 32> identity_key_seed;
    if (!CopyCBORBytestring(&pairing_data.eid_gen_key, state_map, 1) ||
        !CopyCBORBytestring(&pairing_data.psk_gen_key, state_map, 2) ||
        !CopyCBORBytestring(&identity_key_seed, state_map, 3)) {
      return false;
    }

    auth_state_.identity_key = P256KeyFromSeed(identity_key_seed);
    pairing_data.peer_identity.emplace(
        X962PublicKeyOf(auth_state_.identity_key.get()));
    pairing_data.peer_name.emplace(kDeviceName);
    return true;
  }

  void GenerateFreshStateAndStore() {
    device::CableDiscoveryData::V2Data& pairing_data =
        auth_state_.pairing_data.v2.emplace();
    crypto::RandBytes(pairing_data.eid_gen_key);
    crypto::RandBytes(pairing_data.psk_gen_key);

    std::array<uint8_t, 32> identity_key_seed;
    crypto::RandBytes(identity_key_seed);
    auth_state_.identity_key = P256KeyFromSeed(identity_key_seed);
    pairing_data.peer_identity.emplace(
        X962PublicKeyOf(auth_state_.identity_key.get()));
    pairing_data.peer_name.emplace(kDeviceName);

    cbor::Value::MapValue map;
    map.emplace(1, cbor::Value(pairing_data.eid_gen_key));
    map.emplace(2, cbor::Value(pairing_data.psk_gen_key));
    map.emplace(3, cbor::Value(identity_key_seed));

    base::Optional<std::vector<uint8_t>> bytes =
        cbor::Writer::Write(cbor::Value(std::move(map)));
    CHECK(bytes.has_value());

    Java_BLEHandler_setState(env_, ble_handler_, ToJavaByteArray(env_, *bytes));
  }

  static bssl::UniquePtr<EC_KEY> P256KeyFromSeed(
      base::span<const uint8_t, 32> seed) {
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    return bssl::UniquePtr<EC_KEY>(
        EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
  }

  static device::CableAuthenticatorIdentityKey X962PublicKeyOf(
      const EC_KEY* ec_key) {
    device::CableAuthenticatorIdentityKey ret;
    CHECK_EQ(ret.size(),
             EC_POINT_point2oct(EC_KEY_get0_group(ec_key),
                                EC_KEY_get0_public_key(ec_key),
                                POINT_CONVERSION_UNCOMPRESSED, ret.data(),
                                ret.size(), /*ctx=*/nullptr));
    return ret;
  }

  JNIEnv* env_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> ble_handler_;
  AuthenticatorState auth_state_;
  std::unique_ptr<Client> client_;
};

}  // anonymous namespace

// These functions are the entry points for BLEHandler.java calling into C++.

static void JNI_BLEHandler_Start(JNIEnv* env,
                                 const JavaParamRef<jobject>& ble_handler,
                                 const JavaParamRef<jbyteArray>& state_bytes) {
  CableInterface::GetInstance()->Start(env, ble_handler, state_bytes);
}

static void JNI_BLEHandler_Stop(JNIEnv* env) {
  CableInterface::GetInstance()->Stop();
}

static void JNI_BLEHandler_OnQRScanned(JNIEnv* env,
                                       const JavaParamRef<jstring>& jvalue) {
  CableInterface::GetInstance()->OnQRScanned(ConvertJavaStringToUTF8(jvalue));
}

static ScopedJavaLocalRef<jobjectArray> JNI_BLEHandler_Write(
    JNIEnv* env,
    jlong client,
    jint mtu,
    const JavaParamRef<jbyteArray>& data) {
  if (mtu < 0) {
    mtu = 0;
  } else if (mtu > 0xffff) {
    mtu = 0xffff;
  }
  return CableInterface::GetInstance()->Write(client, mtu, data);
}

static void JNI_BLEHandler_OnAuthenticatorAttestationResponse(
    JNIEnv* env,
    jlong client,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jattestation_object) {
  std::vector<uint8_t> client_data_json;
  if (jattestation_object) {
    JavaByteArrayToByteVector(env, jclient_data_json, &client_data_json);
  }
  std::vector<uint8_t> attestation_object;
  if (jattestation_object) {
    JavaByteArrayToByteVector(env, jattestation_object, &attestation_object);
  }
  return CableInterface::GetInstance()->OnMakeCredentialResponse(
      client, ctap_status, client_data_json, attestation_object);
}

static void JNI_BLEHandler_OnAuthenticatorAssertionResponse(
    JNIEnv* env,
    jlong client,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jcredential_id,
    const JavaParamRef<jbyteArray>& jauthenticator_data,
    const JavaParamRef<jbyteArray>& jsignature) {
  std::vector<uint8_t> client_data_json;
  if (jauthenticator_data) {
    JavaByteArrayToByteVector(env, jclient_data_json, &client_data_json);
  }
  std::vector<uint8_t> credential_id;
  if (jcredential_id) {
    JavaByteArrayToByteVector(env, jcredential_id, &credential_id);
  }
  std::vector<uint8_t> authenticator_data;
  if (jauthenticator_data) {
    JavaByteArrayToByteVector(env, jauthenticator_data, &authenticator_data);
  }
  std::vector<uint8_t> signature;
  if (jauthenticator_data) {
    JavaByteArrayToByteVector(env, jsignature, &signature);
  }
  return CableInterface::GetInstance()->OnGetAssertionResponse(
      client, ctap_status, std::move(client_data_json),
      std::move(credential_id), std::move(authenticator_data),
      std::move(signature));
}
