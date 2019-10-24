// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/js_channel_service.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromecast {
namespace {

class JsChannelImpl : public mojom::JsChannel {
 public:
  JsChannelImpl(const std::string& channel, JsChannelCallback callback);
  ~JsChannelImpl() override;

 private:
  void PostMessage(const std::string& message) override;

  std::string channel_;
  JsChannelCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(JsChannelImpl);
};

struct Instance {
  int process_id;
  int routing_id;
  JsClientInstance* instance;
};

using EndpointList = std::vector<Instance>;
static base::LazyInstance<EndpointList>::DestructorAtExit g_instance_list =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

JsChannelService::JsChannelService(int process_id) : process_id_(process_id) {}

JsChannelService::~JsChannelService() = default;

// static
void JsChannelService::Create(
    content::RenderProcessHost* render_process_host,
    mojo::PendingReceiver<mojom::JsChannelBindingProvider> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<JsChannelService>(render_process_host->GetID()),
      std::move(receiver), std::move(task_runner));
}

void JsChannelService::Register(
    int routing_id,
    mojo::PendingRemote<mojom::JsChannelClient> client) {
  if (JsClientInstance::Find(process_id_, routing_id)) {
    LOG(ERROR) << "Duplicate process/routing ID pair!";
  } else {
    // ClientInstance's lifetime is tied to the JsChannelClient mojo
    // channel. It will delete itself when that channel closes.
    new JsClientInstance(process_id_, routing_id, std::move(client));
  }
}

JsClientInstance::JsClientInstance(
    int process_id,
    int routing_id,
    mojo::PendingRemote<mojom::JsChannelClient> client)
    : client_(std::move(client)) {
  client_.set_disconnect_with_reason_handler(
      base::BindRepeating([](JsClientInstance* self, uint32_t err,
                             const std::string& str) { delete self; },
                          base::Unretained(this)));
  g_instance_list.Get().push_back({process_id, routing_id, this});
}

JsClientInstance::~JsClientInstance() {
  auto& list = g_instance_list.Get();
  list.erase(std::find_if(list.begin(), list.end(), [this](const auto& e) {
    return e.instance == this;
  }));
}

// static
JsClientInstance* JsClientInstance::Find(int process_id, int routing_id) {
  for (auto& e : g_instance_list.Get()) {
    if (e.process_id == process_id && e.routing_id == routing_id)
      return e.instance;
  }
  return nullptr;
}

void JsClientInstance::AddChannel(const std::string& channel,
                                  JsChannelCallback callback) {
  mojo::PendingRemote<mojom::JsChannel> channel_remote;
  auto receiver = channel_remote.InitWithNewPipeAndPassReceiver();
  client_->CreateChannel(channel, std::move(channel_remote));

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<JsChannelImpl>(channel, std::move(callback)),
      std::move(receiver));
}

void JsClientInstance::RemoveChannel(const std::string& channel) {
  client_->RemoveChannel(channel);
}

JsChannelImpl::JsChannelImpl(const std::string& channel,
                             JsChannelCallback callback)
    : channel_(channel), callback_(std::move(callback)) {}

JsChannelImpl::~JsChannelImpl() = default;

void JsChannelImpl::PostMessage(const std::string& message) {
  callback_.Run(channel_, message);
}

}  // namespace chromecast
