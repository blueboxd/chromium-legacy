#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"

SyncDisabledChecker::SyncDisabledChecker(syncer::ProfileSyncService* service)
    : SingleClientStatusChangeChecker(service) {}

SyncDisabledChecker::~SyncDisabledChecker() = default;

bool SyncDisabledChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting until sync is disabled."
      << " IsSetupInProgress:" << service()->IsSetupInProgress()
      << " IsFirstSetupComplete:"
      << service()->GetUserSettings()->IsFirstSetupComplete();
  return !service()->IsSetupInProgress() &&
         !service()->GetUserSettings()->IsFirstSetupComplete();
}
