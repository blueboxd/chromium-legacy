// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SERVICE_URLS_H_
#define REMOTING_BASE_SERVICE_URLS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"

namespace remoting {

// This class contains the URLs to the services used by the host (except for
// Gaia, which has its own GaiaUrls class. In debug builds, it allows these URLs
// to be overriden by command line switches, allowing the host process to be
// pointed at alternate/test servers.
class ServiceUrls {
 public:
  static ServiceUrls* GetInstance();

  // Remoting directory REST API URLs.
  const std::string& directory_base_url() const { return directory_base_url_; }
  const std::string& directory_hosts_url() const {
    return directory_hosts_url_;
  }
  const std::string& gcd_base_url() const { return gcd_base_url_; }

  // Remoting directory bot JID (for registering hosts, logging, heartbeats).
  const std::string& directory_bot_jid() const { return directory_bot_jid_; }

  // JID for communicating with GCD.
  const std::string& gcd_jid() const { return gcd_jid_; }

  // ICE config URL.
  const std::string& ice_config_url() const { return ice_config_url_; }

  const std::string& ftl_server_endpoint() const {
    return ftl_server_endpoint_;
  }

  const std::string& remoting_server_endpoint() const {
    return remoting_server_endpoint_;
  }

#if !defined(NDEBUG)
  // Override the directory bot JID for testing.
  void set_directory_bot_jid(const std::string& bot_jid) {
    directory_bot_jid_ = bot_jid;
  }
#endif

 private:
  friend struct base::DefaultSingletonTraits<ServiceUrls>;

  ServiceUrls();
  virtual ~ServiceUrls();

  std::string directory_base_url_;
  std::string directory_hosts_url_;
  std::string gcd_base_url_;
  std::string directory_bot_jid_;
  std::string gcd_jid_;
  std::string ice_config_url_;
  std::string ftl_server_endpoint_;
  std::string remoting_server_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUrls);
};

}  // namespace remoting

#endif  // REMOTING_BASE_SERVICE_URLS_H_
