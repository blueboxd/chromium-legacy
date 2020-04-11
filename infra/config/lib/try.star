"""Library for defining try builders.

The `try_builder` function defined in this module enables defining a builder and
the tryjob verifier for it in the same location. It can also be accessed through
`try_.builder`.

The `tryjob` function specifies the verifier details for a builder. It can also
be accessed through `try_.job`.

The `defaults` struct provides module-level defaults for the arguments to
`try_builder`. The parameters that support module-level defaults have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Can also be accessed through `try_.defaults`.
"""
load('@stdlib//internal/graph.star', 'graph')
load('@stdlib//internal/luci/common.star', 'keys')

load('./builders.star', 'builders')
load('./args.star', 'args')


defaults = args.defaults(
    extends=builders.defaults,
    add_to_list_view = False,
    cq_group = None,
    list_view = args.COMPUTE,
    main_list_view = None,
)


def _sorted_list_view_graph_key(console_name):
  return graph.key('@chromium', '', 'sorted_list_view', console_name)


def _sorted_list_view_impl(ctx, *, console_name):
  key = _sorted_list_view_graph_key(console_name)
  graph.add_node(key)
  graph.add_edge(keys.project(), key)
  return graph.keyset(key)

_sorted_list_view = lucicfg.rule(impl=_sorted_list_view_impl)


def _sort_consoles(ctx):
  milo = ctx.output['luci-milo.cfg']
  for console in milo.consoles:
    graph_key = _sorted_list_view_graph_key(console.id)
    node = graph.node(graph_key)
    if node:
      console.builders = sorted(console.builders, lambda b: list(b.name))
  milo.consoles = sorted(milo.consoles, lambda c: c.id)

lucicfg.generator(_sort_consoles)


def list_view(*, name, **kwargs):
  ret = luci.list_view(
      name = name,
      **kwargs
  )

  _sorted_list_view(
      console_name = name,
  )

  return ret


def tryjob(
    *,
    disable_reuse=None,
    experiment_percentage=None,
    location_regexp=None,
    location_regexp_exclude=None):
  """Specifies the details of a tryjob verifier.

  See https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/lucicfg/doc/README.md#luci.cq_tryjob_verifier
  for details on the arguments. The cq_group parameter supports a module-level
  default that defaults to None.

  Returns:
    A struct that can be passed to the `tryjob` argument of `try_.builder` to
    enable the builder for CQ.
  """
  return struct(
      disable_reuse = disable_reuse,
      experiment_percentage = experiment_percentage,
      location_regexp = location_regexp,
      location_regexp_exclude = location_regexp_exclude,
  )


def try_builder(
    *,
    name,
    add_to_list_view=args.DEFAULT,
    cq_group=args.DEFAULT,
    list_view=args.DEFAULT,
    main_list_view = args.DEFAULT,
    tryjob=None,
    **kwargs):
  """Define a try builder.

  Arguments:
    name - name of the builder, will show up in UIs and logs. Required.
    add_to_list_view - A bool indicating whether an entry should be
      created for the builder in the console identified by
      `list_view`. Supports a module-level default that defaults to
      False.
    cq_group - The CQ group to add the builder to. If tryjob is None, it will
      be added as includable_only.
    list_view - A string identifying the ID of the list view to
      add an entry to. Supports a module-level default that defaults to
      the mastername of the builder, if provided. An entry will be added
      only if `add_to_list_view` is True.
    main_console_view - A string identifying the ID of the main list
      view to add an entry to. Supports a module-level default that
      defaults to None. Note that `add_to_list_view` has no effect on
      creating an entry to the main list view.
    tryjob - A struct containing the details of the tryjob verifier for the
      builder, obtained by calling the `tryjob` function.
  """
  # Define the builder first so that any validation of luci.builder arguments
  # (e.g. bucket) occurs before we try to use it
  ret = builders.builder(
      name = name,
      **kwargs
  )

  bucket = defaults.get_value_from_kwargs('bucket', kwargs)
  builder = '{}/{}'.format(bucket, name)
  cq_group = defaults.get_value('cq_group', cq_group)
  if tryjob != None:
    luci.cq_tryjob_verifier(
        builder = builder,
        cq_group = cq_group,
        disable_reuse = tryjob.disable_reuse,
        experiment_percentage = tryjob.experiment_percentage,
        location_regexp = tryjob.location_regexp,
        location_regexp_exclude = tryjob.location_regexp_exclude,
    )
  else:
    # Allow CQ to trigger this builder if user opts in via CQ-Include-Trybots.
    luci.cq_tryjob_verifier(
        builder = builder,
        cq_group = cq_group,
        includable_only = True,
    )

  add_to_list_view = defaults.get_value('add_to_list_view', add_to_list_view)
  if add_to_list_view:
    list_view = defaults.get_value('list_view', list_view)
    if list_view == args.COMPUTE:
      list_view = defaults.get_value_from_kwargs('mastername', kwargs)

    if list_view:
      add_to_list_view = defaults.get_value(
          'add_to_list_view', add_to_list_view)

      luci.list_view_entry(
          builder = builder,
          list_view = list_view,
      )

  main_list_view = defaults.get_value('main_list_view', main_list_view)
  if main_list_view:
    luci.list_view_entry(
        builder = builder,
        list_view = main_list_view,
    )

  return ret


def blink_builder(*, name, goma_backend = None, **kwargs):
  return try_builder(
      name = name,
      goma_backend = goma_backend,
      mastername = 'tryserver.blink',
      **kwargs
  )


def blink_mac_builder(*, name, **kwargs):
  return blink_builder(
      name = name,
      cores = None,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.MAC_ANY,
      builderless = True,
      ssd = True,
      **kwargs
  )


def chromium_android_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.android',
      **kwargs
  )


def chromium_angle_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = False,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.angle',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def chromium_chromiumos_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      mastername = 'tryserver.chromium.chromiumos',
      goma_backend = builders.goma.backend.RBE_PROD,
      **kwargs
  )


def chromium_dawn_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = False,
      cores = None,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.dawn',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def chromium_linux_builder(*, name, goma_backend=builders.goma.backend.RBE_PROD, **kwargs):
  return try_builder(
      name = name,
      goma_backend = goma_backend,
      mastername = 'tryserver.chromium.linux',
      **kwargs
  )


def chromium_mac_builder(
    *,
    name,
    builderless=True,
    cores=None,
    goma_backend=builders.goma.backend.RBE_PROD,
    os=builders.os.MAC_ANY,
    **kwargs):
  return try_builder(
      name = name,
      cores = cores,
      goma_backend = goma_backend,
      mastername = 'tryserver.chromium.mac',
      os = os,
      builderless = builderless,
      ssd = True,
      **kwargs
  )


def chromium_mac_ios_builder(
    *,
    name,
    executable='recipe:ios/try',
    goma_backend=builders.goma.backend.RBE_PROD,
    properties=None,
    **kwargs):
  if not properties:
    properties = {
      'xcode_build_version': '11c29',
    }
  return try_builder(
      name = name,
      caches = [builders.xcode_cache.x11c29],
      cores = None,
      executable = executable,
      goma_backend = goma_backend,
      mastername = 'tryserver.chromium.mac',
      os = builders.os.MAC_ANY,
      properties = properties,
      **kwargs
  )


def chromium_swangle_builder(*, name, **kwargs):
  return try_builder(
      name = name,
      builderless = True,
      mastername = 'tryserver.chromium.swangle',
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def chromium_swangle_linux_builder(*, name, **kwargs):
  return chromium_swangle_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.LINUX_DEFAULT,
      **kwargs
  )


def chromium_swangle_mac_builder(*, name, **kwargs):
  return chromium_swangle_builder(
      name = name,
      cores = None,
      ssd = None,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.MAC_ANY,
      **kwargs
  )


def chromium_swangle_windows_builder(*, name, **kwargs):
  return chromium_swangle_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      os = builders.os.WINDOWS_DEFAULT,
      **kwargs
  )


def chromium_win_builder(
    *,
    name,
    builderless=True,
    goma_backend=builders.goma.backend.RBE_PROD,
    os=builders.os.WINDOWS_DEFAULT,
    **kwargs):
  return try_builder(
      name = name,
      builderless = builderless,
      goma_backend = goma_backend,
      mastername = 'tryserver.chromium.win',
      os = os,
      **kwargs
  )


def gpu_try_builder(*, name, builderless=False, execution_timeout=6 * time.hour, **kwargs):
  return try_builder(
      name = name,
      builderless = builderless,
      execution_timeout = execution_timeout,
      service_account = 'chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com',
      **kwargs
  )


def gpu_chromium_android_builder(*, name, **kwargs):
  return gpu_try_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.android',
      **kwargs
  )


def gpu_chromium_linux_builder(*, name, **kwargs):
  return gpu_try_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.linux',
      **kwargs
  )


def gpu_chromium_mac_builder(*, name, **kwargs):
  return gpu_try_builder(
      name = name,
      cores = None,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.mac',
      os = builders.os.MAC_ANY,
      **kwargs
  )

def gpu_chromium_win_builder(*, name, os=builders.os.WINDOWS_ANY, **kwargs):
  return gpu_try_builder(
      name = name,
      goma_backend = builders.goma.backend.RBE_PROD,
      mastername = 'tryserver.chromium.win',
      os = os,
      **kwargs
  )


try_ = struct(
    defaults = defaults,
    builder = try_builder,
    job = tryjob,
    list_view = list_view,

    blink_builder = blink_builder,
    blink_mac_builder = blink_mac_builder,
    chromium_android_builder = chromium_android_builder,
    chromium_angle_builder = chromium_angle_builder,
    chromium_chromiumos_builder = chromium_chromiumos_builder,
    chromium_dawn_builder = chromium_dawn_builder,
    chromium_linux_builder = chromium_linux_builder,
    chromium_mac_builder = chromium_mac_builder,
    chromium_mac_ios_builder = chromium_mac_ios_builder,
    chromium_swangle_linux_builder = chromium_swangle_linux_builder,
    chromium_swangle_mac_builder = chromium_swangle_mac_builder,
    chromium_swangle_windows_builder = chromium_swangle_windows_builder,
    chromium_win_builder = chromium_win_builder,
    gpu_chromium_android_builder = gpu_chromium_android_builder,
    gpu_chromium_linux_builder = gpu_chromium_linux_builder,
    gpu_chromium_mac_builder = gpu_chromium_mac_builder,
    gpu_chromium_win_builder = gpu_chromium_win_builder,
)
