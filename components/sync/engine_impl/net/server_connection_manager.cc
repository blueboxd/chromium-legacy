// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/net/server_connection_manager.h"

#include <errno.h>

#include <ostream>

#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine_impl/net/url_translator.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/directory.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace syncer {

using std::string;

namespace {

const char kSyncServerSyncPath[] = "/command/";

#define ENUM_CASE(x)    \
  case HttpResponse::x: \
    return #x;          \
    break

const char* GetServerConnectionCodeString(
    HttpResponse::ServerConnectionCode code) {
  switch (code) {
    ENUM_CASE(NONE);
    ENUM_CASE(CONNECTION_UNAVAILABLE);
    ENUM_CASE(IO_ERROR);
    ENUM_CASE(SYNC_SERVER_ERROR);
    ENUM_CASE(SYNC_AUTH_ERROR);
    ENUM_CASE(SERVER_CONNECTION_OK);
  }
  NOTREACHED();
  return "";
}

#undef ENUM_CASE

}  // namespace

HttpResponse::HttpResponse()
    : server_status(NONE),
      net_error_code(-1),
      http_status_code(-1),
      content_length(-1),
      payload_length(-1) {}

// static
HttpResponse HttpResponse::Uninitialized() {
  return HttpResponse();
}

// static
HttpResponse HttpResponse::ForNetError(int net_error_code) {
  HttpResponse response;
  response.server_status = CONNECTION_UNAVAILABLE;
  response.net_error_code = net_error_code;
  return response;
}

// static
HttpResponse HttpResponse::ForIoError() {
  HttpResponse response;
  response.server_status = IO_ERROR;
  return response;
}

// static
HttpResponse HttpResponse::ForHttpError(int http_status_code) {
  HttpResponse response;
  response.server_status = http_status_code == net::HTTP_UNAUTHORIZED
                               ? SYNC_AUTH_ERROR
                               : SYNC_SERVER_ERROR;
  response.http_status_code = http_status_code;
  return response;
}

// static
HttpResponse HttpResponse::ForSuccess() {
  HttpResponse response;
  response.server_status = SERVER_CONNECTION_OK;
  return response;
}

ServerConnectionManager::Connection::Connection(ServerConnectionManager* scm)
    : scm_(scm) {}

ServerConnectionManager::Connection::~Connection() {}

bool ServerConnectionManager::Connection::ReadBufferResponse(
    string* buffer_out,
    HttpResponse* response,
    bool require_response) {
  if (net::HTTP_OK != response->http_status_code) {
    response->server_status = HttpResponse::SYNC_SERVER_ERROR;
    return false;
  }

  if (require_response && (1 > response->content_length))
    return false;

  const int64_t bytes_read =
      ReadResponse(buffer_out, static_cast<int>(response->content_length));
  if (bytes_read != response->content_length) {
    response->server_status = HttpResponse::IO_ERROR;
    return false;
  }
  return true;
}

namespace {

string StripTrailingSlash(const string& s) {
  int stripped_end_pos = s.size();
  if (s.at(stripped_end_pos - 1) == '/') {
    stripped_end_pos = stripped_end_pos - 1;
  }

  return s.substr(0, stripped_end_pos);
}

}  // namespace

// TODO(crbug.com/951350): Use a GURL instead of string concatenation.
string ServerConnectionManager::Connection::MakeConnectionURL(
    const string& sync_server,
    const string& path,
    bool use_ssl) const {
  string connection_url = (use_ssl ? "https://" : "http://");
  connection_url += sync_server;
  connection_url = StripTrailingSlash(connection_url);
  connection_url += path;

  return connection_url;
}

int ServerConnectionManager::Connection::ReadResponse(string* out_buffer,
                                                      int length) {
  int bytes_read = buffer_.length();
  CHECK(length <= bytes_read);
  out_buffer->assign(buffer_);
  return bytes_read;
}

ServerConnectionManager::ServerConnectionManager(
    const string& server,
    int port,
    bool use_ssl,
    CancelationSignal* cancelation_signal)
    : sync_server_(server),
      sync_server_port_(port),
      use_ssl_(use_ssl),
      proto_sync_path_(kSyncServerSyncPath),
      server_response_(HttpResponse::Uninitialized()),
      cancelation_signal_(cancelation_signal) {}

ServerConnectionManager::~ServerConnectionManager() = default;

std::unique_ptr<ServerConnectionManager::Connection>
ServerConnectionManager::MakeActiveConnection() {
  if (cancelation_signal_->IsSignalled())
    return nullptr;

  return MakeConnection();
}

bool ServerConnectionManager::SetAccessToken(const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!access_token.empty()) {
    access_token_.assign(access_token);
    return true;
  }

  access_token_.clear();

  // The access token could be non-empty in cases like server outage/bug. E.g.
  // token returned by first request is considered invalid by sync server and
  // because of token server's caching policy, etc, same token is returned on
  // second request. Need to notify sync frontend again to request new token,
  // otherwise backend will stay in SYNC_AUTH_ERROR state while frontend thinks
  // everything is fine and takes no actions.
  SetServerResponse(HttpResponse::ForHttpError(net::HTTP_UNAUTHORIZED));
  return false;
}

void ServerConnectionManager::ClearAccessToken() {
  access_token_.clear();
}

void ServerConnectionManager::SetServerResponse(
    const HttpResponse& server_response) {
  // Notify only if the server status changed, except for SYNC_AUTH_ERROR: In
  // that case, always notify in order to poke observers to do something about
  // it.
  bool notify =
      (server_response.server_status == HttpResponse::SYNC_AUTH_ERROR ||
       server_response_.server_status != server_response.server_status);
  server_response_ = server_response;
  if (notify) {
    NotifyStatusChanged();
  }
}

void ServerConnectionManager::NotifyStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : listeners_)
    observer.OnServerConnectionEvent(
        ServerConnectionEvent(server_response_.server_status));
}

bool ServerConnectionManager::PostBufferWithCachedAuth(
    PostBufferParams* params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  string path =
      MakeSyncServerPath(proto_sync_path(), MakeSyncQueryString(client_id_));
  bool result = PostBufferToPath(params, path, access_token_);
  SetServerResponse(params->response);
  return result;
}

bool ServerConnectionManager::PostBufferToPath(PostBufferParams* params,
                                               const string& path,
                                               const string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (access_token.empty()) {
    params->response.server_status = HttpResponse::SYNC_AUTH_ERROR;
    // Print a log to distinguish this "known failure" from others.
    DVLOG(1) << "ServerConnectionManager forcing SYNC_AUTH_ERROR due to missing"
                " access token";
    return false;
  }

  std::unique_ptr<Connection> connection = MakeActiveConnection();
  if (!connection) {
    params->response.server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    return false;
  }

  // Note that |post| may be aborted by now, which will just cause Init to fail
  // with CONNECTION_UNAVAILABLE.
  bool ok = connection->Init(path.c_str(), access_token, params->buffer_in,
                             &params->response);

  if (params->response.server_status == HttpResponse::SYNC_AUTH_ERROR) {
    access_token_.clear();
  }

  if (!ok || net::HTTP_OK != params->response.http_status_code)
    return false;

  if (connection->ReadBufferResponse(&params->buffer_out, &params->response,
                                     true)) {
    params->response.server_status = HttpResponse::SERVER_CONNECTION_OK;
    return true;
  }
  return false;
}

void ServerConnectionManager::AddListener(
    ServerConnectionEventListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void ServerConnectionManager::RemoveListener(
    ServerConnectionEventListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

std::unique_ptr<ServerConnectionManager::Connection>
ServerConnectionManager::MakeConnection() {
  return nullptr;  // For testing.
}

std::ostream& operator<<(std::ostream& s, const struct HttpResponse& hr) {
  s << " Response Code (bogus on error): " << hr.http_status_code;
  s << " Content-Length (bogus on error): " << hr.content_length;
  s << " Server Status: " << GetServerConnectionCodeString(hr.server_status);
  return s;
}

}  // namespace syncer
