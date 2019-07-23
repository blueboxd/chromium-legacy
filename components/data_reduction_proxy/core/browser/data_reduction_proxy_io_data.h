// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/network/public/mojom/network_context.mojom.h"

class PrefService;

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactoryInfo;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyConfigServiceClient;
class DataReductionProxyConfigurator;
class DataReductionProxyServer;
class DataReductionProxyService;
class NetworkPropertiesManager;

// Contains and initializes all Data Reduction Proxy objects that operate on
// the IO thread.
class DataReductionProxyIOData : public mojom::DataReductionProxy {
 public:
  // Constructs a DataReductionProxyIOData object. |enabled| sets the initial
  // state of the Data Reduction Proxy.
  DataReductionProxyIOData(
      Client client,
      PrefService* prefs,
      network::NetworkConnectionTracker* network_connection_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      bool enabled,
      const std::string& user_agent,
      const std::string& channel);

  ~DataReductionProxyIOData() override;

  // Performs UI thread specific shutdown logic.
  void ShutdownOnUIThread();

  // Sets the Data Reduction Proxy service after it has been created.
  // Virtual for testing.
  virtual void SetDataReductionProxyService(
      base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service,
      const std::string& user_agent);

  // Sets user defined preferences for how the Data Reduction Proxy
  // configuration should be set. |at_startup| is true only
  // when DataReductionProxySettings is initialized.
  void SetProxyPrefs(bool enabled, bool at_startup);

  // Applies a serialized Data Reduction Proxy configuration.
  void SetDataReductionProxyConfiguration(const std::string& serialized_config);

  // When triggering previews, prevent long term black list rules. Overridden in
  // testing.
  virtual void SetIgnoreLongTermBlackListRules(
      bool ignore_long_term_black_list_rules);

  // Bridge methods to safely call to the UI thread objects.
  void UpdateDataUseForHost(int64_t network_bytes,
                            int64_t original_bytes,
                            const std::string& host);
  void UpdateContentLengths(
      int64_t data_used,
      int64_t original_size,
      bool data_reduction_proxy_enabled,
      DataReductionProxyRequestType request_type,
      const std::string& mime_type,
      bool is_user_traffic,
      data_use_measurement::DataUseUserData::DataUseContentType content_type,
      int32_t service_hash_code);

  // Returns true if the Data Reduction Proxy is enabled and false otherwise.
  bool IsEnabled() const;

  // Changes the reporting fraction for the pingback service to
  // |pingback_reporting_fraction|. Overridden in testing.
  virtual void SetPingbackReportingFraction(float pingback_reporting_fraction);

  // Called when the user clears the browsing history.
  void DeleteBrowsingHistory(const base::Time start, const base::Time end);

  // Notifies |this| that the user has requested to clear the browser
  // cache. This method is not called if only a subset of site entries are
  // cleared.
  void OnCacheCleared(const base::Time start, const base::Time end);

  // Forwards proxy authentication headers to the UI thread.
  void UpdateProxyRequestHeaders(const net::HttpRequestHeaders& headers);

  // Notifies |this| that there there is a change in the effective connection
  // type.
  void OnEffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // Notifies |this| that there there is a change in the HTTP RTT estimate.
  void OnRTTOrThroughputEstimatesComputed(base::TimeDelta http_rtt);

  // Returns the current estimate of the effective connection type.
  net::EffectiveConnectionType GetEffectiveConnectionType() const;

  // Binds to a config client that can be used to update Data Reduction Proxy
  // settings when the network service is enabled.
  void SetCustomProxyConfigClient(
      network::mojom::CustomProxyConfigClientPtrInfo config_client_info);

  // Various accessor methods.
  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  DataReductionProxyConfig* config() const {
    return config_.get();
  }

  DataReductionProxyRequestOptions* request_options() const {
    return request_options_.get();
  }

  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }

  mojom::DataReductionProxyThrottleConfigPtr CreateThrottleConfig() const;

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() const {
    return io_task_runner_;
  }

  // The production channel of this build.
  std::string channel() const { return channel_; }

  // The Client type of this build.
  Client client() const { return client_; }

  // mojom::DataReductionProxy implementation:
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override;
  void AddThrottleConfigObserver(
      mojom::DataReductionProxyThrottleConfigObserverPtr observer) override;
  void Clone(mojom::DataReductionProxyRequest request) override;
  void AddThrottleConfigObserverInfo(
      mojom::DataReductionProxyThrottleConfigObserverPtrInfo observer);

 private:
  friend class TestDataReductionProxyIOData;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest, TestConstruction);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyIODataTest,
                           TestResetBadProxyListOnDisableDataSaver);

  // Used for testing.
  DataReductionProxyIOData(
      PrefService* prefs,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Initializes the weak pointer to |this| on the IO thread. It must be done
  // on the IO thread, since it is used for posting tasks from the UI thread
  // to IO thread objects in a thread safe manner.
  void InitializeOnIOThread(const std::string& user_agent);

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Stores an int64_t value in preferences storage.
  void SetInt64Pref(const std::string& pref_path, int64_t value);

  // Stores a string value in preferences storage.
  void SetStringPref(const std::string& pref_path, const std::string& value);

  // Stores a serialized Data Reduction Proxy configuration in preferences
  // storage.
  void StoreSerializedConfig(const std::string& serialized_config);

  // Creates a config using |proxies_for_http| that can be sent to the
  // NetworkContext.
  network::mojom::CustomProxyConfigPtr CreateCustomProxyConfig(
      bool is_warmup_url,
      const std::vector<DataReductionProxyServer>& proxies_for_http) const;

  // Called when the list of proxies changes.
  void OnProxyConfigUpdated();

  // Should be called whenever there is a possible change to the custom proxy
  // config.
  void UpdateCustomProxyConfig();

  // Should be called whenever there is a possible change to the throttle
  // config.
  void UpdateThrottleConfig();

  // The type of Data Reduction Proxy client.
  const Client client_;

  // Parameters including DNS names and allowable configurations.
  std::unique_ptr<DataReductionProxyConfig> config_;

  // Setter of the Data Reduction Proxy-specific proxy configuration.
  std::unique_ptr<DataReductionProxyConfigurator> configurator_;

  // Data Reduction Proxy objects with a UI based lifetime.
  base::WeakPtr<DataReductionProxyService> service_;

  // Constructs credentials suitable for authenticating the client.
  std::unique_ptr<DataReductionProxyRequestOptions> request_options_;

  // Requests new Data Reduction Proxy configurations from a remote service.
  // May be null.
  std::unique_ptr<DataReductionProxyConfigServiceClient> config_client_;

  // Watches for network connection changes.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // IO and UI task runners, respectively.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Whether the Data Reduction Proxy has been enabled or not by the user. In
  // practice, this can be overridden by the command line.
  bool enabled_;

  // The network::SharedURLLoaderFactoryInfo used for making URL requests.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;

  // The production channel of this build.
  const std::string channel_;

  // Created on the UI thread. Guaranteed to be destroyed on IO thread if the
  // IO thread is still available at the time of destruction. If the IO thread
  // is unavailable, then the destruction will happen on the UI thread.
  std::unique_ptr<NetworkPropertiesManager> network_properties_manager_;

  // Current estimate of the effective connection type.
  net::EffectiveConnectionType effective_connection_type_;

  network::mojom::CustomProxyConfigClientPtr proxy_config_client_;

  mojo::BindingSet<mojom::DataReductionProxy> drp_bindings_;

  mojo::InterfacePtrSet<mojom::DataReductionProxyThrottleConfigObserver>
      drp_throttle_config_observers_;

  base::WeakPtrFactory<DataReductionProxyIOData> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyIOData);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_IO_DATA_H_
