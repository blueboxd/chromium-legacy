// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/updater_constants.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

class Observer : public update_client::UpdateClient::Observer {
 public:
  explicit Observer(scoped_refptr<update_client::UpdateClient> update_client)
      : update_client_(update_client) {}

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override {
    update_client_->GetCrxUpdateState(id, &crx_update_item_);
  }

  const update_client::CrxUpdateItem& crx_update_item() const {
    return crx_update_item_;
  }

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  update_client::CrxUpdateItem crx_update_item_;
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

}  // namespace

int UpdateApps() {
  auto installer = base::MakeRefCounted<Installer>(kUpdaterAppId);
  installer->FindInstallOfApp();
  const auto component = installer->MakeCrxComponent();

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop runloop;
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  auto config = base::MakeRefCounted<Configurator>();
  {
    base::ScopedDisallowBlocking no_blocking_allowed;

    auto update_client = update_client::UpdateClientFactory(config);

    Observer observer(update_client);
    update_client->AddObserver(&observer);

    const std::vector<std::string> ids = {installer->app_id()};
    update_client->Update(
        ids,
        base::BindOnce(
            [](const update_client::CrxComponent& component,
               const std::vector<std::string>& ids)
                -> std::vector<base::Optional<update_client::CrxComponent>> {
              DCHECK_EQ(1u, ids.size());
              return {component};
            },
            component),
        false,
        base::BindOnce(
            [](base::OnceClosure closure, update_client::Error error) {
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, base::BindOnce(
                                 [](base::OnceClosure quit_closure) {
                                   std::move(quit_closure).Run();
                                 },
                                 std::move(closure)));
            },
            runloop.QuitWhenIdleClosure()));

    runloop.Run();

    const auto& update_item = observer.crx_update_item();
    switch (update_item.state) {
      case update_client::ComponentState::kUpdated:
        VLOG(1) << "Update success.";
        break;
      case update_client::ComponentState::kUpToDate:
        VLOG(1) << "No updates.";
        break;
      case update_client::ComponentState::kUpdateError:
        VLOG(1) << "Updater error: " << update_item.error_code << ".";
        break;
      default:
        NOTREACHED();
        break;
    }
    update_client->RemoveObserver(&observer);
    update_client = nullptr;
  }

  {
    base::RunLoop runloop;
    config->GetPrefService()->CommitPendingWrite(base::BindOnce(
        [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
        runloop.QuitWhenIdleClosure()));
    runloop.Run();
  }

  return 0;
}

}  // namespace updater
