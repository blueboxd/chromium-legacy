// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/install_app.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "base/win/atl.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/install_progress_observer.h"
#include "chrome/updater/win/setup/setup.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/splash_screen.h"
#include "chrome/updater/win/ui/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

// TODO(sorin): remove the hardcoding of the application name.
// https://crbug.com/1014298
constexpr base::char16 kAppNameChrome[] = L"Google Chrome";

class InstallAppController;

// Implements a simple inter-thread communication protocol based on Windows
// messages exchanged between the application installer and its UI.
//
// Since the installer code and the UI code execute on different threads, the
// installer can't invoke directly functions exposed by the UI.
// This class translates a function call made by the installer to any of the
// overriden virtual functions into posting a window message with specific
// arguments to the progress window implemented by the UI. Handling such
// message consists of unpacking the arguments of the message, and invoking
// the UI code in the correct thread, which is the thread that owns the window.
class InstallProgressObserverIPC : public InstallProgressObserver {
 public:
  // Used as an inter-thread communication mechanism between the installer and
  // UI threads.
  static constexpr unsigned int WM_PROGRESS_WINDOW_IPC = WM_APP + 1;

  explicit InstallProgressObserverIPC(ui::ProgressWnd* progress_wnd);
  ~InstallProgressObserverIPC() override;

  // Called by the window proc when a specific application message is processed
  // by the progress window. This call always occurs in the context of the
  // thread which owns the window.
  void Invoke(WPARAM wparam, LPARAM lparam);

  // Overrides for InstallProgressObserver.
  // Called by the application installer code on the main udpater thread.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const base::string16& app_id,
                         const base::string16& app_name,
                         const base::string16& version_string) override;
  void OnWaitingToDownload(const base::string16& app_id,
                           const base::string16& app_name) override;
  void OnDownloading(const base::string16& app_id,
                     const base::string16& app_name,
                     int time_remaining_ms,
                     int pos) override;
  void OnWaitingRetryDownload(const base::string16& app_id,
                              const base::string16& app_name,
                              const base::Time& next_retry_time) override;
  void OnWaitingToInstall(const base::string16& app_id,
                          const base::string16& app_name,
                          bool* can_start_install) override;
  void OnInstalling(const base::string16& app_id,
                    const base::string16& app_name,
                    int time_remaining_ms,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

 private:
  enum class IPCAppMessages {
    kOnCheckingForUpdate = 0,
    kOnUpdateAvailable,
    kOnWaitingToDownload,
    kOnDownloading,
    kOnWaitingRetryDownload,
    kOnWaitingToInstall,
    kOnInstalling,
    kOnPause,
    kOnComplete,
  };

  struct ParamOnUpdateAvailable {
    ParamOnUpdateAvailable();
    ParamOnUpdateAvailable(const ParamOnUpdateAvailable&) = delete;
    ParamOnUpdateAvailable& operator=(const ParamOnUpdateAvailable&) = delete;

    base::string16 app_id;
    base::string16 app_name;
    base::string16 version_string;
  };

  struct ParamOnDownloading {
    ParamOnDownloading();
    ParamOnDownloading(const ParamOnDownloading&) = delete;
    ParamOnDownloading& operator=(const ParamOnDownloading&) = delete;

    base::string16 app_id;
    base::string16 app_name;
    int time_remaining_ms = 0;
    int pos = 0;
  };

  struct ParamOnWaitingToInstall {
    ParamOnWaitingToInstall();
    ParamOnWaitingToInstall(const ParamOnWaitingToInstall&) = delete;
    ParamOnWaitingToInstall& operator=(const ParamOnWaitingToInstall&) = delete;

    base::string16 app_id;
    base::string16 app_name;
  };

  struct ParamOnInstalling {
    ParamOnInstalling();
    ParamOnInstalling(const ParamOnInstalling&) = delete;
    ParamOnInstalling& operator=(const ParamOnInstalling&) = delete;

    base::string16 app_id;
    base::string16 app_name;
    int time_remaining_ms = 0;
    int pos = 0;
  };

  struct ParamOnComplete {
    ParamOnComplete();
    ParamOnComplete(const ParamOnComplete&) = delete;
    ParamOnComplete& operator=(const ParamOnComplete&) = delete;

    ObserverCompletionInfo observer_info;
  };

  THREAD_CHECKER(thread_checker_);

  // This member is not owned by this class.
  ui::ProgressWnd* progress_wnd_ = nullptr;

  // The thread id of the thread which owns the |ProgressWnd|.
  int window_thread_id_ = 0;

  InstallProgressObserverIPC(const InstallProgressObserverIPC&) = delete;
  InstallProgressObserverIPC& operator=(const InstallProgressObserverIPC&) =
      delete;
};

InstallProgressObserverIPC::ParamOnUpdateAvailable::ParamOnUpdateAvailable() =
    default;
InstallProgressObserverIPC::ParamOnDownloading::ParamOnDownloading() = default;
InstallProgressObserverIPC::ParamOnWaitingToInstall::ParamOnWaitingToInstall() =
    default;
InstallProgressObserverIPC::ParamOnInstalling::ParamOnInstalling() = default;
InstallProgressObserverIPC::ParamOnComplete::ParamOnComplete() = default;

InstallProgressObserverIPC::InstallProgressObserverIPC(
    ui::ProgressWnd* progress_wnd)
    : progress_wnd_(progress_wnd),
      window_thread_id_(
          ::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr)) {
  DCHECK(progress_wnd);
  DCHECK(progress_wnd->m_hWnd);
  DCHECK(IsWindow(progress_wnd->m_hWnd));
}
InstallProgressObserverIPC::~InstallProgressObserverIPC() = default;

void InstallProgressObserverIPC::OnCheckingForUpdate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnCheckingForUpdate),
                      0);
}

void InstallProgressObserverIPC::OnUpdateAvailable(
    const base::string16& app_id,
    const base::string16& app_name,
    const base::string16& version_string) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available =
      std::make_unique<ParamOnUpdateAvailable>();
  param_on_update_available->app_id = app_id;
  param_on_update_available->app_name = app_name;
  param_on_update_available->version_string = version_string;
  ::PostThreadMessage(
      window_thread_id_, WM_PROGRESS_WINDOW_IPC,
      static_cast<WPARAM>(IPCAppMessages::kOnUpdateAvailable),
      reinterpret_cast<LPARAM>(param_on_update_available.release()));
}

void InstallProgressObserverIPC::OnWaitingToDownload(
    const base::string16& app_id,
    const base::string16& app_name) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnDownloading(const base::string16& app_id,
                                               const base::string16& app_name,
                                               int time_remaining_ms,
                                               int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnDownloading> param_on_downloading =
      std::make_unique<ParamOnDownloading>();
  param_on_downloading->app_id = app_id;
  param_on_downloading->app_name = app_name;
  param_on_downloading->time_remaining_ms = time_remaining_ms;
  param_on_downloading->pos = pos;
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnDownloading),
                      reinterpret_cast<LPARAM>(param_on_downloading.release()));
}

void InstallProgressObserverIPC::OnWaitingRetryDownload(
    const base::string16& app_id,
    const base::string16& app_name,
    const base::Time& next_retry_time) {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnWaitingToInstall(
    const base::string16& app_id,
    const base::string16& app_name,
    bool* can_start_install) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnWaitingToInstall> param_on_waiting_to_install =
      std::make_unique<ParamOnWaitingToInstall>();
  param_on_waiting_to_install->app_id = app_id;
  param_on_waiting_to_install->app_name = app_name;
  ::PostThreadMessage(
      window_thread_id_, WM_PROGRESS_WINDOW_IPC,
      static_cast<WPARAM>(IPCAppMessages::kOnWaitingToInstall),
      reinterpret_cast<LPARAM>(param_on_waiting_to_install.release()));
}

void InstallProgressObserverIPC::OnInstalling(const base::string16& app_id,
                                              const base::string16& app_name,
                                              int time_remaining_ms,
                                              int pos) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(sorin): implement progress, https://crbug.com/1014594.
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnInstalling> param_on_installing =
      std::make_unique<ParamOnInstalling>();
  param_on_installing->app_id = app_id;
  param_on_installing->app_name = app_name;
  param_on_installing->time_remaining_ms = time_remaining_ms;
  param_on_installing->pos = pos;
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnInstalling),
                      reinterpret_cast<LPARAM>(param_on_installing.release()));
}

void InstallProgressObserverIPC::OnPause() {
  NOTREACHED();
}

void InstallProgressObserverIPC::OnComplete(
    const ObserverCompletionInfo& observer_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(progress_wnd_);
  std::unique_ptr<ParamOnComplete> param_on_complete =
      std::make_unique<ParamOnComplete>();
  param_on_complete->observer_info = observer_info;
  ::PostThreadMessage(window_thread_id_, WM_PROGRESS_WINDOW_IPC,
                      static_cast<WPARAM>(IPCAppMessages::kOnComplete),
                      reinterpret_cast<LPARAM>(param_on_complete.release()));
}

void InstallProgressObserverIPC::Invoke(WPARAM wparam, LPARAM lparam) {
  DCHECK_EQ(::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr),
            ::GetCurrentThreadId());
  auto* observer = static_cast<InstallProgressObserver*>(progress_wnd_);
  switch (static_cast<IPCAppMessages>(wparam)) {
    case IPCAppMessages::kOnCheckingForUpdate:
      observer->OnCheckingForUpdate();
      break;
    case IPCAppMessages::kOnUpdateAvailable: {
      std::unique_ptr<ParamOnUpdateAvailable> param_on_update_available(
          reinterpret_cast<ParamOnUpdateAvailable*>(lparam));
      observer->OnUpdateAvailable(param_on_update_available->app_id,
                                  param_on_update_available->app_name,
                                  param_on_update_available->version_string);
      break;
    }
    case IPCAppMessages::kOnDownloading: {
      std::unique_ptr<ParamOnDownloading> param_on_downloading(
          reinterpret_cast<ParamOnDownloading*>(lparam));
      observer->OnDownloading(
          param_on_downloading->app_id, param_on_downloading->app_name,
          param_on_downloading->time_remaining_ms, param_on_downloading->pos);
      break;
    }
    case IPCAppMessages::kOnWaitingToInstall: {
      std::unique_ptr<ParamOnWaitingToInstall> param_on_waiting_to_install(
          reinterpret_cast<ParamOnWaitingToInstall*>(lparam));
      // TODO(sorin): implement cancelling of an install. crbug.com/1014591
      bool can_install = false;
      observer->OnWaitingToInstall(param_on_waiting_to_install->app_id,
                                   param_on_waiting_to_install->app_name,
                                   &can_install);
      break;
    }
    case IPCAppMessages::kOnInstalling: {
      std::unique_ptr<ParamOnInstalling> param_on_installing(
          reinterpret_cast<ParamOnInstalling*>(lparam));
      observer->OnInstalling(
          param_on_installing->app_id, param_on_installing->app_name,
          param_on_installing->time_remaining_ms, param_on_installing->pos);
      break;
    }
    case IPCAppMessages::kOnComplete: {
      std::unique_ptr<ParamOnComplete> param_on_complete(
          reinterpret_cast<ParamOnComplete*>(lparam));
      observer->OnComplete(param_on_complete->observer_info);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// Implements installing a single application by invoking the code in
// |update_client|, listening to |update_client| and UI events, and
// driving the UI code by calling the functions exposed by
// |InstallProgressObserver|. This class receives state changes for an install
// and it notifies the UI, which is an observer of this class.
//
// The UI code can't run in a thread where the message loop is an instance of
// |base::MessageLoop|. |base::MessageLoop| does not handle all the messages
// needed by the UI, since the UI is written in terms of WTL, and it requires
// a |WTL::MessageLoop| to work, for example, accelerators, dialog messages,
// TAB key, etc are all handled by WTL. Therefore, the UI code runs on its own
// thread. This thread owns all the UI objects, which must be created and
// destroyed on this thread. The rest of the code in this class runs on
// the updater main thread.
//
// This class controls the lifetime of the UI thread. Once the UI thread is
// created, it is going to run a message loop until the main thread initiates
// its teardown by posting a WM_QUIT message to it. This makes the UI message
// loop exit, and after that, the execution flow returns to the scheduler task,
// which has been running the UI message loop. Upon its completion, the task
// posts a reply to the main thread, which makes the main thread exit its run
// loop, and then the main thread returns to the destructor of this class,
// and destructs its class members.
class InstallAppController
    : public base::RefCountedThreadSafe<InstallAppController>,
      public ui::ProgressWndEvents,
      public WTL::CMessageFilter {
 public:
  explicit InstallAppController(
      scoped_refptr<update_client::Configurator> configurator);

  InstallAppController(const InstallAppController&) = delete;
  InstallAppController& operator=(const InstallAppController&) = delete;

  void InstallApp(const std::string& app_id,
                  base::OnceCallback<void(int)> callback);

 private:
  friend class base::RefCountedThreadSafe<InstallAppController>;

  ~InstallAppController() override;

  // Overrides for OmahaWndEvents. These functions are called on the UI thread.
  void DoClose() override {}
  void DoExit() override;

  // Overrides for CompleteWndEvents. This function is called on the UI thread.
  bool DoLaunchBrowser(const base::string16& url) override { return false; }

  // Overrides for ProgressWndEvents. These functions are called on the UI
  // thread.
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<base::string16>& urls) override {
    return false;
  }
  bool DoReboot() override { return false; }
  void DoCancel() override {}

  // Overrides for WTL::CMessageFilter.
  BOOL PreTranslateMessage(MSG* msg) override;

  // These functions are called on the UI thread.
  void InitializeUI();
  void RunUI();

  // These functions are called on the main updater thread.
  void DoInstallApp();
  void InstallComplete();
  void HandleInstallResult(const update_client::CrxUpdateItem& update_item);
  void FlushPrefs();

  // Returns the thread id of the thread which owns the progress window.
  DWORD GetUIThreadID() const;

  // Receives the state changes during handling of the Install function call.
  void StateChange(update_client::CrxUpdateItem crx_update_item);

  SEQUENCE_CHECKER(sequence_checker_);

  // Provides an execution environment for the updater main thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Provides an execution environment for the UI code. Typically, it runs
  // a single task which is the UI run loop.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // The application ID and the associated application name. The application
  // name is displayed by the UI and it must be i18n.
  std::string app_id_;
  const base::string16 app_name_;

  // The |update_client| objects and dependencies.
  scoped_refptr<update_client::Configurator> config_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;

  // The message loop associated with the UI.
  std::unique_ptr<WTL::CMessageLoop> ui_message_loop_;

  // The progress window.
  std::unique_ptr<ui::ProgressWnd> progress_wnd_;

  // The adapter for the inter-thread calls between the updater main thread
  // and the UI thread.
  std::unique_ptr<InstallProgressObserverIPC> install_progress_observer_ipc_;

  // Called when InstallApp is done.
  base::OnceCallback<void(int)> callback_;
};

// TODO(sorin): fix the hardcoding of the application name.
// https:crbug.com/1014298
InstallAppController::InstallAppController(
    scoped_refptr<update_client::Configurator> configurator)
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      ui_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      app_name_(kAppNameChrome),
      config_(configurator),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())) {}
InstallAppController::~InstallAppController() = default;

void InstallAppController::InstallApp(const std::string& app_id,
                                      base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  app_id_ = app_id;
  callback_ = std::move(callback);

  ui_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&InstallAppController::InitializeUI, this),
      base::BindOnce(&InstallAppController::DoInstallApp, this));
}

void InstallAppController::DoInstallApp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // At this point, the UI has been initialized, which means the UI can be
  // used from now on as an observer of the application install. The task
  // below runs the UI message loop for the UI until it exits, because
  // a WM_QUIT message has been posted to it.
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&InstallAppController::RunUI, this));

  update_client_ = update_client::UpdateClientFactory(config_);

  install_progress_observer_ipc_ =
      std::make_unique<InstallProgressObserverIPC>(progress_wnd_.get());

  update_client_->Install(
      app_id_,
      base::BindOnce(
          [](scoped_refptr<PersistedData> persisted_data,
             const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            DCHECK_EQ(1u, ids.size());
            return {base::MakeRefCounted<Installer>(ids[0], persisted_data)
                        ->MakeCrxComponent()};
          },
          persisted_data_),
      base::BindRepeating(&InstallAppController::StateChange, this),
      base::BindOnce(
          [](scoped_refptr<InstallAppController> install_app_controller,
             update_client::Error error) {
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(&InstallAppController::InstallComplete,
                               install_app_controller));
          },
          base::WrapRefCounted(this)));
}

// This function is invoked after the |update_client::Install| call has been
// fully handled and the state associated with the application ID has been
// destroyed. The caller of |update_client::Install| must listen to
// Observer::OnEvent to get completion status instead of querying the status
// by calling UpdateClient::GetCrxUpdateState.
void InstallAppController::InstallComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushPrefs();
  install_progress_observer_ipc_ = nullptr;
  update_client_ = nullptr;
}

void InstallAppController::StateChange(
    update_client::CrxUpdateItem crx_update_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(install_progress_observer_ipc_);

  CHECK_EQ(app_id_, crx_update_item.id);

  const auto app_id = base::ASCIIToUTF16(app_id_);
  switch (crx_update_item.state) {
    case update_client::ComponentState::kChecking:
      install_progress_observer_ipc_->OnCheckingForUpdate();
      break;

    case update_client::ComponentState::kCanUpdate:
      install_progress_observer_ipc_->OnUpdateAvailable(
          app_id, app_name_,
          base::ASCIIToUTF16(crx_update_item.next_version.GetString()));
      break;

    case update_client::ComponentState::kDownloading:
      // TODO(sorin): handle progress and time remaining.
      // https://crbug.com/1014590
      install_progress_observer_ipc_->OnDownloading(app_id, app_name_, -1, 0);
      break;

    case update_client::ComponentState::kUpdating: {
      // TODO(sorin): handle the install cancellation.
      // https://crbug.com/1014591
      bool can_start_install = false;
      install_progress_observer_ipc_->OnWaitingToInstall(app_id, app_name_,
                                                         &can_start_install);

      // TODO(sorin): handle progress and time remaining.
      // https://crbug.com/1014594
      install_progress_observer_ipc_->OnInstalling(app_id, app_name_, 0, 0);
      break;
    }

    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpToDate:
    case update_client::ComponentState::kUpdateError:
      HandleInstallResult(crx_update_item);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void InstallAppController::HandleInstallResult(
    const update_client::CrxUpdateItem& update_item) {
  CompletionCodes completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
  base::string16 completion_text;
  switch (update_item.state) {
    case update_client::ComponentState::kUpdated:
      VLOG(1) << "Update success.";
      completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
      ui::LoadString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY, &completion_text);
      break;
    case update_client::ComponentState::kUpToDate:
      VLOG(1) << "No updates.";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      ui::LoadString(IDS_NO_UPDATE_RESPONSE, &completion_text);
      break;
    case update_client::ComponentState::kUpdateError:
      VLOG(1) << "Updater error: " << update_item.error_code << ".";
      completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
      ui::LoadString(IDS_INSTALL_FAILED, &completion_text);
      break;
    default:
      NOTREACHED();
      break;
  }

  ObserverCompletionInfo observer_info;
  observer_info.completion_code = completion_code;
  observer_info.completion_text = completion_text;
  // TODO(sorin): implement handling the help URL. https://crbug.com/1014622
  observer_info.help_url = L"http://www.google.com";
  // TODO(sorin): implement the installer API and provide the
  // application info in the observer info. https://crbug.com/1014630
  observer_info.apps_info.push_back({});
  install_progress_observer_ipc_->OnComplete(observer_info);
}

// Creates and shows the progress window. The window has thread affinity. It
// must be created, process its messages, and be destroyed on the same thread.
void InstallAppController::InitializeUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  base::ScopedDisallowBlocking no_blocking_allowed_on_ui_thread;

  ui_message_loop_ = std::make_unique<WTL::CMessageLoop>();
  ui_message_loop_->AddMessageFilter(this);
  progress_wnd_ =
      std::make_unique<ui::ProgressWnd>(ui_message_loop_.get(), nullptr);
  progress_wnd_->SetEventSink(this);
  progress_wnd_->Initialize();
  progress_wnd_->Show();
}

void InstallAppController::RunUI() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());

  ui_message_loop_->Run();
  ui_message_loop_->RemoveMessageFilter(this);

  // This object is owned by the UI thread must be destroyed on this thread.
  progress_wnd_ = nullptr;

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback_), 0));
}

void InstallAppController::DoExit() {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
}

BOOL InstallAppController::PreTranslateMessage(MSG* msg) {
  DCHECK_EQ(GetUIThreadID(), GetCurrentThreadId());
  if (msg->message == InstallProgressObserverIPC::WM_PROGRESS_WINDOW_IPC) {
    install_progress_observer_ipc_->Invoke(msg->wParam, msg->lParam);
    return true;
  }
  return false;
}

void InstallAppController::FlushPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

DWORD InstallAppController::GetUIThreadID() const {
  DCHECK(progress_wnd_);
  return ::GetWindowThreadProcessId(progress_wnd_->m_hWnd, nullptr);
}

}  // namespace

// Sets the updater up, shows up a splash screen, then installs an application
// while displaying the UI progress window.
class AppInstall : public App {
 public:
  AppInstall() = default;

 private:
  ~AppInstall() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  void SetupDone(int result);

  scoped_refptr<Configurator> config_;
  scoped_refptr<InstallAppController> app_install_controller_;

  // The splash screen has a fading effect. That means that the splash screen
  // needs to be alive for a while, until the fading effect is over.
  std::unique_ptr<ui::SplashScreen> splash_screen_;
};

void AppInstall::Initialize() {
  base::i18n::InitializeICU();
  config_ = base::MakeRefCounted<Configurator>();
}

void AppInstall::Uninitialize() {
  PrefsCommitPendingWrites(config_->GetPrefService());
}

void AppInstall::FirstTaskRun() {
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  splash_screen_ = std::make_unique<ui::SplashScreen>(kAppNameChrome);
  splash_screen_->Show();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([]() { return Setup(false); }),
      base::BindOnce(
          [](ui::SplashScreen* splash_screen,
             base::OnceCallback<void(int)> done, int result) {
            splash_screen->Dismiss(base::BindOnce(std::move(done), result));
          },
          splash_screen_.get(), base::BindOnce(&AppInstall::SetupDone, this)));
}

// Updates the prefs if the setup is successful, then continue installing
// the application if --appid is specified on the command line.
void AppInstall::SetupDone(int result) {
  if (result != 0) {
    Shutdown(result);
    return;
  }

  base::MakeRefCounted<PersistedData>(config_->GetPrefService())
      ->SetProductVersion(kUpdaterAppId, base::Version(UPDATER_VERSION_STRING));

  const auto app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kAppIdSwitch);
  if (app_id.empty()) {
    Shutdown(result);
    return;
  }

  app_install_controller_ = base::MakeRefCounted<InstallAppController>(config_);
  app_install_controller_->InstallApp(
      app_id, base::BindOnce(&AppInstall::Shutdown, this));
}

scoped_refptr<App> AppInstallInstance() {
  // TODO(sorin) "--install" must be run with "--single-process" until
  // crbug.com/1053729 is resolved.
  DCHECK(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kSingleProcessSwitch));
  return AppInstance<AppInstall>();
}

}  // namespace updater
