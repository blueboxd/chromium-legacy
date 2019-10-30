// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_ntlm.h"

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/ssl/ssl_info.h"

namespace net {

namespace {

uint64_t GetMSTime() {
  return base::Time::Now().since_origin().InMicroseconds() * 10;
}

void GenerateRandom(uint8_t* output, size_t n) {
  base::RandBytes(output, n);
}

}  // namespace

int HttpAuthHandlerNTLM::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  if (reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  // NOTE: Default credentials are not supported for the portable implementation
  // of NTLM.
  std::unique_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNTLM(http_auth_preferences()));
  if (!tmp_handler->InitFromChallenge(challenge, target, ssl_info, origin,
                                      net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

// static
HttpAuthHandlerNTLM::GetMSTimeProc HttpAuthHandlerNTLM::get_ms_time_proc_ =
    GetMSTime;

// static
HttpAuthHandlerNTLM::GenerateRandomProc
    HttpAuthHandlerNTLM::generate_random_proc_ = GenerateRandom;

// static
HttpAuthHandlerNTLM::HostNameProc HttpAuthHandlerNTLM::get_host_name_proc_ =
    GetHostName;

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM(
    const HttpAuthPreferences* http_auth_preferences)
    : ntlm_client_(ntlm::NtlmFeatures(
          http_auth_preferences ? http_auth_preferences->NtlmV2Enabled()
                                : true)) {}

bool HttpAuthHandlerNTLM::NeedsIdentity() {
  // This gets called for each round-trip.  Only require identity on
  // the first call (when auth_data_ is empty).  On subsequent calls,
  // we use the initially established identity.
  return auth_data_.empty();
}

bool HttpAuthHandlerNTLM::AllowsDefaultCredentials() {
  // Default credentials are not supported in the portable implementation of
  // NTLM, but are supported in the SSPI implementation.
  return false;
}

int HttpAuthHandlerNTLM::InitializeBeforeFirstChallenge() {
  return OK;
}

int HttpAuthHandlerNTLM::GenerateAuthTokenImpl(
    const AuthCredentials* credentials,
    const HttpRequestInfo* request,
    CompletionOnceCallback callback,
    std::string* auth_token) {
  // TODO(cbentzel): Shouldn't be hitting this case.
  if (!credentials) {
    LOG(ERROR) << "Username and password are expected to be non-nullptr.";
    return ERR_MISSING_AUTH_CREDENTIALS;
  }

  // The username may be in the form "DOMAIN\user".  Parse it into the two
  // components.
  base::string16 domain;
  base::string16 user;
  const base::string16& username = credentials->username();
  const base::char16 backslash_character = '\\';
  size_t backslash_idx = username.find(backslash_character);
  if (backslash_idx == base::string16::npos) {
    user = username;
  } else {
    domain = username.substr(0, backslash_idx);
    user = username.substr(backslash_idx + 1);
  }
  domain_ = domain;
  credentials_.Set(user, credentials->password());

  std::string decoded_auth_data;
  if (auth_data_.empty()) {
    // There is no |auth_data_| because the client sends the first message.
    int rv = InitializeBeforeFirstChallenge();
    if (rv != OK)
      return rv;
  } else {
    // When |auth_data_| is present it contains the Challenge message.
    if (!base::Base64Decode(auth_data_, &decoded_auth_data)) {
      LOG(ERROR) << "Unexpected problem Base64 decoding.";
      return ERR_UNEXPECTED;
    }
  }

  std::vector<uint8_t> next_token =
      GetNextToken(base::as_bytes(base::make_span(decoded_auth_data)));
  if (next_token.empty())
    return ERR_UNEXPECTED;

  // Base64 encode data in output buffer and prepend "NTLM ".
  std::string encode_output;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(next_token.data()),
                        next_token.size()),
      &encode_output);

  *auth_token = std::string("NTLM ") + encode_output;
  return OK;
}

HttpAuthHandlerNTLM::~HttpAuthHandlerNTLM() = default;

// static
HttpAuthHandlerNTLM::GetMSTimeProc HttpAuthHandlerNTLM::SetGetMSTimeProc(
    GetMSTimeProc proc) {
  GetMSTimeProc old_proc = get_ms_time_proc_;
  get_ms_time_proc_ = proc;
  return old_proc;
}

// static
HttpAuthHandlerNTLM::GenerateRandomProc
HttpAuthHandlerNTLM::SetGenerateRandomProc(GenerateRandomProc proc) {
  GenerateRandomProc old_proc = generate_random_proc_;
  generate_random_proc_ = proc;
  return old_proc;
}

// static
HttpAuthHandlerNTLM::HostNameProc HttpAuthHandlerNTLM::SetHostNameProc(
    HostNameProc proc) {
  HostNameProc old_proc = get_host_name_proc_;
  get_host_name_proc_ = proc;
  return old_proc;
}

std::vector<uint8_t> HttpAuthHandlerNTLM::GetNextToken(
    base::span<const uint8_t> in_token) {
  // If in_token is non-empty, then assume it contains a challenge message,
  // and generate the Authenticate message in reply. Otherwise return the
  // Negotiate message.
  if (in_token.empty()) {
    return ntlm_client_.GetNegotiateMessage();
  }

  std::string hostname = get_host_name_proc_();
  if (hostname.empty())
    return {};
  uint8_t client_challenge[8];
  generate_random_proc_(client_challenge, 8);
  uint64_t client_time = get_ms_time_proc_();

  return ntlm_client_.GenerateAuthenticateMessage(
      domain_, credentials_.username(), credentials_.password(), hostname,
      channel_bindings_, CreateSPN(origin_), client_time, client_challenge,
      in_token);
}

HttpAuth::AuthorizationResult HttpAuthHandlerNTLM::ParseChallenge(
    HttpAuthChallengeTokenizer* tok,
    bool initial_challenge) {
  auth_data_.clear();

  // Verify the challenge's auth-scheme.
  if (!base::LowerCaseEqualsASCII(tok->scheme(), kNtlmAuthScheme))
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;

  std::string base64_param = tok->base64_param();
  if (base64_param.empty()) {
    if (!initial_challenge)
      return HttpAuth::AUTHORIZATION_RESULT_REJECT;
    return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
  } else {
    if (initial_challenge)
      return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  }

  auth_data_ = base64_param;
  return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

}  // namespace net
