// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_NET_SERVER_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_NET_SERVER_CONNECTION_MANAGER_H_

#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/sync/syncable/syncable_id.h"

namespace syncer {

class CancelationSignal;

// HttpResponse gathers the relevant output properties of an HTTP request.
// Depending on the value of the server_status code, response_code, and
// content_length may not be valid.
struct HttpResponse {
  enum ServerConnectionCode {
    // For uninitialized state.
    NONE,

    // CONNECTION_UNAVAILABLE means either the request got canceled or it
    // encountered a network error.
    CONNECTION_UNAVAILABLE,

    // IO_ERROR is returned when reading/writing to a buffer has failed.
    IO_ERROR,

    // SYNC_SERVER_ERROR is returned when the HTTP status code indicates that
    // a non-auth error has occurred.
    SYNC_SERVER_ERROR,

    // SYNC_AUTH_ERROR is returned when the HTTP status code indicates that an
    // auth error has occurred (i.e. a 401).
    // TODO(crbug.com/842096, crbug.com/951350): Remove this and instead use
    // SYNC_SERVER_ERROR plus |http_status_code| == 401.
    SYNC_AUTH_ERROR,

    // SERVER_CONNECTION_OK is returned when request was handled correctly.
    SERVER_CONNECTION_OK,
  };

  // Identifies the type of failure, if any.
  ServerConnectionCode server_status;

  // The network error code.
  int net_error_code;

  // The HTTP Status code.
  int http_status_code;

  // The value of the Content-length header.
  int64_t content_length;

  // The size of a download request's payload.
  int64_t payload_length;

  static HttpResponse Uninitialized();
  static HttpResponse ForNetError(int net_error_code);
  static HttpResponse ForIoError();
  static HttpResponse ForHttpError(int http_status_code);
  static HttpResponse ForSuccess();

 private:
  // Private to prevent accidental usage. Use Uninitialized() if you really need
  // a "default" instance.
  HttpResponse();
};

struct ServerConnectionEvent {
  HttpResponse::ServerConnectionCode connection_code;
  explicit ServerConnectionEvent(HttpResponse::ServerConnectionCode code)
      : connection_code(code) {}
};

class ServerConnectionEventListener {
 public:
  virtual void OnServerConnectionEvent(const ServerConnectionEvent& event) = 0;

 protected:
  virtual ~ServerConnectionEventListener() {}
};

// Use this class to interact with the sync server.
// The ServerConnectionManager currently supports POSTing protocol buffers.
//
class ServerConnectionManager {
 public:
  // buffer_in - will be POSTed
  // buffer_out - string will be overwritten with response
  struct PostBufferParams {
    std::string buffer_in;
    std::string buffer_out;
    HttpResponse response = HttpResponse::Uninitialized();
  };

  // Abstract class providing network-layer functionality to the
  // ServerConnectionManager. Subclasses implement this using an HTTP stack of
  // their choice.
  class Connection {
   public:
    explicit Connection(ServerConnectionManager* scm);
    virtual ~Connection();

    // Called to initialize and perform an HTTP POST.
    // TODO(crbug.com/951350): Return the HttpResponse by value. It's not
    // obvious what the boolean return value means. (True means success or HTTP
    // error, false means canceled or network error.)
    virtual bool Init(const char* path,
                      const std::string& access_token,
                      const std::string& payload,
                      HttpResponse* response) = 0;

    bool ReadBufferResponse(std::string* buffer_out,
                            HttpResponse* response,
                            bool require_response);

   protected:
    std::string MakeConnectionURL(const std::string& sync_server,
                                  const std::string& path,
                                  bool use_ssl) const;

    void GetServerParams(std::string* server,
                         int* server_port,
                         bool* use_ssl) const {
      server->assign(scm_->sync_server_);
      *server_port = scm_->sync_server_port_;
      *use_ssl = scm_->use_ssl_;
    }

    std::string buffer_;

   private:
    int ReadResponse(std::string* buffer, int length);

    ServerConnectionManager* scm_;
  };

  ServerConnectionManager(const std::string& server,
                          int port,
                          bool use_ssl,
                          CancelationSignal* cancelation_signal);

  virtual ~ServerConnectionManager();

  // POSTS buffer_in and reads a response into buffer_out. Uses our currently
  // set access token in our headers.
  //
  // Returns true if executed successfully.
  virtual bool PostBufferWithCachedAuth(PostBufferParams* params);

  void AddListener(ServerConnectionEventListener* listener);
  void RemoveListener(ServerConnectionEventListener* listener);

  inline HttpResponse::ServerConnectionCode server_status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.server_status;
  }

  inline int net_error_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.net_error_code;
  }

  inline int http_status_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.http_status_code;
  }

  const std::string client_id() const { return client_id_; }

  void set_client_id(const std::string& client_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(client_id_.empty());
    client_id_.assign(client_id);
  }

  // Sets a new access token. If |access_token| is empty, the current token is
  // invalidated and cleared. Returns false if the server is in authentication
  // error state.
  bool SetAccessToken(const std::string& access_token);

  bool HasInvalidAccessToken() { return access_token_.empty(); }

 protected:
  inline std::string proto_sync_path() const { return proto_sync_path_; }

  // Updates |server_response_| and notifies listeners if the server status
  // changed.
  void SetServerResponse(const HttpResponse& server_response);

  // Factory method to create an Connection object we can use for communication
  // with the server. The returned object must not outlive |*this|.
  virtual std::unique_ptr<Connection> MakeConnection();

  // NOTE: Tests rely on this protected function being virtual.
  //
  // Internal PostBuffer base function.
  virtual bool PostBufferToPath(PostBufferParams*,
                                const std::string& path,
                                const std::string& access_token);

  void ClearAccessToken();

  // Helper to check terminated flags and build a Connection object. If this
  // ServerConnectionManager has been terminated, this will return null. The
  // returned object must not outlive |*this|.
  std::unique_ptr<Connection> MakeActiveConnection();

 private:
  void NotifyStatusChanged();

  // The sync_server_ is the server that requests will be made to.
  std::string sync_server_;

  // The sync_server_port_ is the port that HTTP requests will be made on.
  int sync_server_port_;

  // The unique id of the user's client.
  std::string client_id_;

  // Indicates whether or not requests should be made using HTTPS.
  bool use_ssl_;

  // The paths we post to.
  std::string proto_sync_path_;

  // The access token to use in authenticated requests.
  std::string access_token_;

  base::ObserverList<ServerConnectionEventListener>::Unchecked listeners_;

  HttpResponse server_response_;

  SEQUENCE_CHECKER(sequence_checker_);

  CancelationSignal* const cancelation_signal_;

  DISALLOW_COPY_AND_ASSIGN(ServerConnectionManager);
};

std::ostream& operator<<(std::ostream& s, const struct HttpResponse& hr);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_NET_SERVER_CONNECTION_MANAGER_H_
