vars = struct(
    # Switch this to False for branches
    is_master = True,
    ref = 'refs/heads/master',
    ci_bucket = 'ci',
    ci_poller = 'master-gitiles-trigger',
    main_console_name = 'main',
    main_console_title = 'Chromium Main Console',
    try_bucket = 'try',
    cq_group = 'cq',
    cq_ref_regexp = 'refs/heads/.+',
    # Delete this line for branches
    tree_status_host = 'chromium-status.appspot.com/',
)
