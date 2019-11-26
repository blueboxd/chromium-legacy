// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include "build/branding_buildflags.h"

// The recovery component is built and used by Google Chrome only.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"

// The action handler is defined for Windows only. crbug/687231.
#if defined(OS_WIN)
#include <windows.h>
#include <wrl/client.h>

#include <iterator>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/install_static/install_util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/patcher.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/version_info/version_info.h"
#endif  // OS_WIN

// This component is behind a Finch experiment. To enable the registration of
// the component, run Chrome with --enable-features=ImprovedRecoveryComponent.
namespace component_updater {

#if defined(OS_WIN)

namespace {

const base::FilePath::CharType kRecoveryFileName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

constexpr base::TaskTraits kTaskTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// This task joins a process, hence .WithBaseSyncPrimitives().
constexpr base::TaskTraits kTaskTraitsRunCommand = {
    base::ThreadPool(), base::MayBlock(), base::WithBaseSyncPrimitives(),
    base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

// Returns the current browser version.
std::string GetBrowserVersion() {
  return version_info::GetVersion().GetString();
}

// Returns the Chrome's appid registered with Google Update for updates.
std::string GetBrowserAppId() {
  return base::UTF16ToUTF8(install_static::GetAppGuid());
}

// Instantiates the elevator service, calls its elevator interface, then
// blocks waiting for the recovery processes to exit. Returns the result
// of the recovery as a tuple.
std::tuple<bool, int, int> RunRecoveryCRXElevated(
    const base::FilePath& crx_path,
    const std::string& browser_appid,
    const std::string& browser_version,
    const std::string& session_id) {
  Microsoft::WRL::ComPtr<IElevator> elevator;
  HRESULT hr = CoCreateInstance(
      install_static::GetElevatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      install_static::GetElevatorIid(), IID_PPV_ARGS_Helper(&elevator));
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  hr = CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  ULONG_PTR proc_handle = 0;
  hr = elevator->RunRecoveryCRXElevated(
      crx_path.value().c_str(), base::UTF8ToWide(browser_appid).c_str(),
      base::UTF8ToWide(browser_version).c_str(),
      base::UTF8ToWide(session_id).c_str(), base::Process::Current().Pid(),
      &proc_handle);
  if (FAILED(hr))
    return {false, static_cast<int>(hr), 0};

  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  base::Process process(reinterpret_cast<base::ProcessHandle>(proc_handle));
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  return {succeeded, exit_code, 0};
}

// Handles the |run| action for the recovery component for Windows.
//
// The recovery component for Windows consists of a Windows executable
// program named |ChromeRecovery.exe|, wrapped inside a CRX container, which
// is itself contained inside a component updater CRX payload.
// In other words, looking from outside, the component updater installs and
// updates a CRX, containing a CRX, containing an EXE.

// The |ActionHandler| is responsible for unpacking the inner CRX described
// above, and running the executable program inside it.
//
// The |ActionHandler::Handle| function is invoked as a result of an |action|
// element present in the update response for the recovery component. Note that
// the |action| element can be present in the update response even if there are
// no updates for the recovery component.
//
// When Chrome is installed per-system, the CRX is being handed over to
// a system elevator, which unpacks the CRX in a secure location of the
// file system, and runs the recovery program with system privileges.
//
// When Chrome is installed per-user, the CRX is unpacked in a temporary
// directory for the user, and the recovery program runs with normal user
// privileges.
class ActionHandler : public update_client::ActionHandler {
 public:
  ActionHandler();

 private:
  ~ActionHandler() override;

  // Overrides for update_client::ActionHandler. |action| is an absolute file
  // path to a CRX to be unpacked. |session_id| contains the session id
  // corresponding to the current update transaction. The session id is passed
  // as a command line argument to the recovery program, and sent as part of
  // the completion pings during the actual recovery.
  void Handle(const base::FilePath& action,
              const std::string& session_id,
              Callback callback) override;

  // Calls the elevator service to handle the CRX. Since the invocation of
  // the elevator service consists of several Windows COM IPC calls, a
  // certain type of task runner is necessary to initialize a COM apartment.
  void RunElevatedInSTA();

  void Unpack();
  void UnpackComplete(const update_client::ComponentUnpacker::Result& result);
  void RunCommand(const base::CommandLine& cmdline);
  base::CommandLine MakeCommandLine(const base::FilePath& unpack_path) const;
  void WaitForCommand(base::Process process);

  SEQUENCE_CHECKER(sequence_checker_);

  // True if Chrome is installed per-user.
  bool is_per_user_install_ = component_updater::IsPerUserInstall();

  // Executes tasks in the context of the sequence which created this object.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunnerHandle::Get();

  // Contains the CRX specified as a run action.
  base::FilePath crx_path_;

  // The session id of the update transaction, as defined by |update_client|.
  std::string session_id_;

  // Called when the action is handled.
  Callback callback_;

  // Contains the path where the action CRX is unpacked in the per-user case.
  base::FilePath unpack_path_;

  ActionHandler(const ActionHandler&) = delete;
  ActionHandler& operator=(const ActionHandler&) = delete;
};

ActionHandler::ActionHandler() = default;

ActionHandler::~ActionHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ActionHandler::Handle(const base::FilePath& action,
                           const std::string& session_id,
                           Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  crx_path_ = action;
  session_id_ = session_id;
  callback_ = std::move(callback);

  // Unpacks the CRX for this user or hands it over to the system elevator.
  if (is_per_user_install_) {
    base::CreateSequencedTaskRunner(kTaskTraits)
        ->PostTask(FROM_HERE, base::BindOnce(&ActionHandler::Unpack, this));
  } else {
    base::CreateCOMSTATaskRunner(
        kTaskTraitsRunCommand,
        base::SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ActionHandler::RunElevatedInSTA, this));
  }
}

void ActionHandler::RunElevatedInSTA() {
  bool succeeded = false;
  int error_code = 0;
  int extra_code = 0;
  std::tie(succeeded, error_code, extra_code) = RunRecoveryCRXElevated(
      crx_path_, GetBrowserAppId(), GetBrowserVersion(), session_id_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), succeeded, error_code, extra_code));
}

void ActionHandler::Unpack() {
  // The key hash for the inner CRX.
  const std::vector<uint8_t> key_hash = {
      0x5f, 0x94, 0xe0, 0x3c, 0x64, 0x30, 0x9f, 0xbc, 0xfe, 0x00, 0x9a,
      0x27, 0x3e, 0x52, 0xbf, 0xa5, 0x84, 0xb9, 0xb3, 0x75, 0x07, 0x29,
      0xde, 0xfa, 0x32, 0x76, 0xd9, 0x93, 0xb5, 0xa3, 0xce, 0x02};
  auto unzipper = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
                      base::BindRepeating(&unzip::LaunchUnzipper))
                      ->Create();
  auto unpacker = base::MakeRefCounted<update_client::ComponentUnpacker>(
      key_hash, crx_path_, nullptr, std::move(unzipper), nullptr,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF);
  unpacker->Unpack(base::BindOnce(&ActionHandler::UnpackComplete, this));
}

void ActionHandler::UnpackComplete(
    const update_client::ComponentUnpacker::Result& result) {
  if (result.error != update_client::UnpackerError::kNone) {
    DCHECK(!base::DirectoryExists(result.unpack_path));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), false,
                       static_cast<int>(result.error), result.extended_error));
    return;
  }

  unpack_path_ = result.unpack_path;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ActionHandler::RunCommand, this,
                                MakeCommandLine(result.unpack_path)));
}

void ActionHandler::RunCommand(const base::CommandLine& cmdline) {
  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(cmdline, options);
  base::PostTask(
      FROM_HERE, kTaskTraitsRunCommand,
      base::BindOnce(&ActionHandler::WaitForCommand, this, std::move(process)));
}

base::CommandLine ActionHandler::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  base::CommandLine command_line(unpack_path.Append(kRecoveryFileName));
  if (!is_per_user_install_)
    command_line.AppendSwitch("system");
  command_line.AppendSwitchASCII("browser-version", GetBrowserVersion());
  command_line.AppendSwitchASCII("sessionid", session_id_);
  const auto app_guid = GetBrowserAppId();
  if (!app_guid.empty())
    command_line.AppendSwitchASCII("appguid", app_guid);
  VLOG(1) << "run action: " << command_line.GetCommandLineString();
  return command_line;
}

void ActionHandler::WaitForCommand(base::Process process) {
  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  base::DeleteFileRecursively(unpack_path_);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), succeeded, exit_code, 0));
}

}  // namespace

#endif  // OS_WIN

// The SHA256 of the SubjectPublicKeyInfo used to sign the component CRX.
// The component id is: ihnlcenocehgdaegdmhbidjhnhdchfmm
constexpr uint8_t kRecoveryImprovedPublicKeySHA256[32] = {
    0x87, 0xdb, 0x24, 0xde, 0x24, 0x76, 0x30, 0x46, 0x3c, 0x71, 0x83,
    0x97, 0xd7, 0x32, 0x75, 0xcc, 0xd5, 0x7f, 0xec, 0x09, 0x60, 0x6d,
    0x20, 0xc3, 0x81, 0xd7, 0xce, 0x7b, 0x10, 0x15, 0x44, 0xd1};

RecoveryImprovedInstallerPolicy::RecoveryImprovedInstallerPolicy(
    PrefService* prefs)
    : prefs_(prefs) {}

RecoveryImprovedInstallerPolicy::~RecoveryImprovedInstallerPolicy() {}

bool RecoveryImprovedInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool RecoveryImprovedInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
RecoveryImprovedInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void RecoveryImprovedInstallerPolicy::OnCustomUninstall() {}

void RecoveryImprovedInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "RecoveryImproved component is ready.";
}

// Called during startup and installation before ComponentReady().
bool RecoveryImprovedInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath RecoveryImprovedInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("RecoveryImproved"));
}

void RecoveryImprovedInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kRecoveryImprovedPublicKeySHA256),
               std::end(kRecoveryImprovedPublicKeySHA256));
}

std::string RecoveryImprovedInstallerPolicy::GetName() const {
  return "Chrome Improved Recovery";
}

update_client::InstallerAttributes
RecoveryImprovedInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

std::vector<std::string> RecoveryImprovedInstallerPolicy::GetMimeTypes() const {
  return {};
}

void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {
// TODO(sorin): enable recovery component for macOS. crbug/687231.
#if defined(OS_WIN)
  DVLOG(1) << "Registering RecoveryImproved component.";

  // |cus| keeps a reference to the |installer| in the CrxComponent instance.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<RecoveryImprovedInstallerPolicy>(prefs),
      base::MakeRefCounted<ActionHandler>());
  installer->Register(cus, base::OnceClosure());
#endif
}

}  // namespace component_updater

#else
namespace component_updater {
void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {}
}  // namespace component_updater
#endif  // GOOGLE_CHROME_BRANDING
