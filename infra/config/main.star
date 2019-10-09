#!/usr/bin/env lucicfg
# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = 'generated',
    tracked_files = [
        'commit-queue.cfg',
        'cr-buildbucket.cfg',
        'luci-logdog.cfg',
        'luci-milo.cfg',
        'luci-notify.cfg',
        'luci-scheduler.cfg',
        'project.cfg',
        'tricium-prod.cfg',
    ],
    fail_on_warnings = True,
)

# Copy the not-yet migrated files to the generated outputs
# TODO(https://crbug.com/1011908) Migrate the configuration in these files to starlark
[lucicfg.emit(dest = f, data = io.read_file(f)) for f in (
    'commit-queue.cfg',
    'cr-buildbucket.cfg',
    'luci-milo.cfg',
    'luci-notify.cfg',
    'luci-scheduler.cfg',
)]

# Just copy tricium-prod.cfg to the generated outputs
lucicfg.emit(
    dest = 'tricium-prod.cfg',
    data = io.read_file('tricium-prod.cfg'),
)

luci.project(
    name = 'chromium',
    logdog = 'luci-logdog.appspot.com',
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
            ],
            groups = 'all',
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = 'luci-logdog-chromium-writers',
        ),
    ],
)

luci.logdog(
    gs_bucket = 'chromium-luci-logdog',
)
