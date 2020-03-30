"""Library for defining builders.

The `builder` function defined in this module simplifies setting all of the
dimensions and many of the properties used for chromium builders by providing
direct arguments for them rather than requiring them to appear as part of a
dict. This simplifies creating wrapper functions that need to fix or override
the default value of specific dimensions or property fields without having to
handle merging dictionaries. Can also be accessed through `builders.builder`.

The `defaults` struct provides module-level defaults for the arguments to
`builder`. Each parameter of `builder` besides `name` and `kwargs` have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Additionally, the module-level defaults defined for
use with `luci.builder` can be accessed through `defaults`. Finally module-level
defaults are provided for the `bucket` and `executable` arguments, removing the
need to create a wrapper function just to set those default values for a bucket.
Can also be accessed through `builders.defaults`.

The `cpu`, `os`, and `goma` module members are structs that provide constants
for use with the corresponding arguments to `builder`. Can also be accessed
through `builders.cpu`, `builders.os` and `builders.goma` respectively.
"""

load('./args.star', 'args')


################################################################################
# Constants for use with the builder function                                  #
################################################################################

# The cpu constants to be used with the builder function
cpu = struct(
    X86 = 'x86',
    X86_64 = 'x86-64',
)


# The category for an os: a more generic grouping than specific OS versions that
# can be used for computing defaults
os_category = struct(
    ANDROID = 'Android',
    LINUX = 'Linux',
    MAC = 'Mac',
    WINDOWS = 'Windows',
)

# The os constants to be used for the os parameter of the builder function
# The *_DEFAULT members enable distinguishing between a use that runs the
# "current" version of the OS and a use that runs against a specific version
# that happens to be the "current" version
def os_enum(dimension, category):
  return struct(dimension=dimension, category=category)

os = struct(
    ANDROID = os_enum('Android', os_category.ANDROID),

    LINUX_TRUSTY = os_enum('Ubuntu-14.04', os_category.LINUX),
    LINUX_XENIAL = os_enum('Ubuntu-16.04', os_category.LINUX),
    LINUX_DEFAULT = os_enum('Ubuntu-16.04', os_category.LINUX),

    MAC_10_12 = os_enum('Mac-10.12', os_category.MAC),
    MAC_10_13 = os_enum('Mac-10.13', os_category.MAC),
    MAC_10_14 = os_enum('Mac-10.14', os_category.MAC),
    MAC_10_15 = os_enum('Mac-10.15', os_category.MAC),
    MAC_DEFAULT = os_enum('Mac-10.13', os_category.MAC),
    MAC_ANY = os_enum('Mac', os_category.MAC),

    WINDOWS_7 = os_enum('Windows-7', os_category.WINDOWS),
    WINDOWS_8_1 = os_enum('Windows-8.1', os_category.WINDOWS),
    WINDOWS_10 = os_enum('Windows-10', os_category.WINDOWS),
    WINDOWS_DEFAULT = os_enum('Windows-10', os_category.WINDOWS),
    WINDOWS_ANY = os_enum('Windows', os_category.WINDOWS),
)


# The constants to be used for the goma_backend and goma_jobs parameters of the
# builder function
goma = struct(
    backend = struct(
        RBE_PROD = {
            'server_host': 'goma.chromium.org',
            'rpc_extra_params': '?prod',
        },
        RBE_STAGING = {
            'server_host': 'staging-goma.chromium.org',
            'rpc_extra_params': '?staging',
        },
        RBE_TOT = {
            'server_host': 'staging-goma.chromium.org',
            'rpc_extra_params': '?tot',
        },
    ),
    jobs = struct(
        J50 = 50,

        # This is for 4 cores mac. -j40 is too small, especially for clobber
        # builder.
        J80 = 80,

        # This is for tryservers becoming slow and critical path of patch
        # landing.
        J150 = 150,

        # This is for tryservers becoming very slow and critical path of patch
        # landing.
        J300 = 300,

        # CI builders (of which are few) may use high number of concurrent Goma
        # jobs.
        # IMPORTANT: when
        #  * bumping number of jobs below, or
        #  * adding this mixin to many builders at once, or
        #  * adding this mixin to a builder with many concurrent builds
        # get review from Goma team.
        MANY_JOBS_FOR_CI = 500,

        # For load testing of the execution backend
        LOAD_TESTING_J1000 = 1000,
        LOAD_TESTING_J2000 = 2000,
    ),
)


def xcode_enum(cache_name, cache_path):
  return swarming.cache(name=cache_name, path=cache_path)


# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode_cache = struct(
   x10e1001 = xcode_enum('xcode_ios_10e1001', 'xcode_ios_10e1001.app'),
   x11a1027 = xcode_enum('xcode_ios_11a1027', 'xcode_ios_11a1027.app'),
   x11c505wk = xcode_enum('xcode_ios_11c505wk', 'xcode_ios_11c505wk.app'),
   x11c29 = xcode_enum('xcode_ios_11c29', 'xcode_ios_11c29.app'),
   x11m382q = xcode_enum('xcode_ios_11m382q', 'xcode_ios_11m382q.app'),
)


################################################################################
# Implementation details                                                       #
################################################################################

_DEFAULT_BUILDERLESS_OS_CATEGORIES = [os_category.LINUX]


def _chromium_tests_property(*, bucketed_triggers):
  chromium_tests = {}

  bucketed_triggers = defaults.get_value('bucketed_triggers', bucketed_triggers)
  if bucketed_triggers:
    chromium_tests['bucketed_triggers'] = True

  return chromium_tests or None


def _goma_property(*, goma_backend, goma_debug, goma_enable_ats, goma_jobs, os):
  goma_properties = {}

  goma_backend = defaults.get_value('goma_backend', goma_backend)
  if goma_backend != None:
    goma_properties.update(goma_backend)

  goma_debug = defaults.get_value('goma_debug', goma_debug)
  if goma_debug:
    goma_properties['debug'] = True

  goma_enable_ats = defaults.get_value('goma_enable_ats', goma_enable_ats)
  # TODO(crbug.com/1040754): Remove this flag.
  if goma_enable_ats == args.COMPUTE:
    goma_enable_ats = (
        os and os.category in (os_category.LINUX, os_category.WINDOWS) and
        goma_backend in (goma.backend.RBE_TOT, goma.backend.RBE_STAGING,
                         goma.backend.RBE_PROD))
  if goma_enable_ats:
    goma_properties['enable_ats'] = True

  goma_jobs = defaults.get_value('goma_jobs', goma_jobs)
  if goma_jobs != None:
    goma_properties['jobs'] = goma_jobs

  return goma_properties or None


def _code_coverage_property(*, use_clang_coverage, use_java_coverage):
  code_coverage = {}

  use_clang_coverage = defaults.get_value(
      'use_clang_coverage', use_clang_coverage)
  if use_clang_coverage:
    code_coverage['use_clang_coverage'] = True

  use_java_coverage = defaults.get_value('use_java_coverage', use_java_coverage)
  if use_java_coverage:
    code_coverage['use_java_coverage'] = True

  return code_coverage or None


################################################################################
# Builder defaults and function                                                #
################################################################################

# The module-level defaults to use with the builder function
defaults = args.defaults(
    luci.builder.defaults,

    # Our custom arguments
    auto_builder_dimension = args.COMPUTE,
    builderless = args.COMPUTE,
    bucketed_triggers = False,
    configure_kitchen = False,
    cores = None,
    cpu = None,
    goma_backend = None,
    goma_debug = False,
    goma_enable_ats = args.COMPUTE,
    goma_jobs = None,
    mastername = None,
    os = None,
    pool = None,
    ssd = args.COMPUTE,
    use_clang_coverage = False,
    use_java_coverage = False,
    should_exonerate_flaky_failures = False,

    # Provide vars for bucket and executable so users don't have to
    # unnecessarily make wrapper functions
    bucket = args.COMPUTE,
    executable = args.COMPUTE,
    triggered_by = args.COMPUTE,

    # Forward on luci.builder.defaults so users have a consistent interface
    **{a: getattr(luci.builder.defaults, a) for a in dir(luci.builder.defaults)}
)

def builder(
    *,
    name,
    bucket=args.DEFAULT,
    executable=args.DEFAULT,
    triggered_by=args.DEFAULT,
    os=args.DEFAULT,
    builderless=args.DEFAULT,
    auto_builder_dimension=args.DEFAULT,
    cores=args.DEFAULT,
    cpu=args.DEFAULT,
    mastername=args.DEFAULT,
    pool=args.DEFAULT,
    ssd=args.DEFAULT,
    bucketed_triggers=args.DEFAULT,
    configure_kitchen=args.DEFAULT,
    goma_backend=args.DEFAULT,
    goma_debug=args.DEFAULT,
    goma_enable_ats=args.DEFAULT,
    goma_jobs=args.DEFAULT,
    use_clang_coverage=args.DEFAULT,
    use_java_coverage=args.DEFAULT,
    should_exonerate_flaky_failures=args.DEFAULT,
    **kwargs):
  """Define a builder.

  For all of the optional parameters defined by this method, passing None will
  prevent the emission of any dimensions or property fields associated with that
  parameter.

  All parameters defined by this function except for `name` and `kwargs` support
  module-level defaults. The `defaults` struct defined in this module has an
  attribute with a `lucicfg.var` for all of the fields defined here as well as
  all of the parameters of `luci.builder` that support module-level defaults.

  See https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/lucicfg/doc/README.md#luci.builder
  for more information.

  Arguments:
    * name - name of the builder, will show up in UIs and logs. Required.
    * bucket - a bucket the build is in, see luci.bucket(...) rule. Required
      (may be specified by module-level default).
    * executable - an executable to run, e.g. a luci.recipe(...). Required (may
      be specified by module-level default).
    * os - a member of the `os` enum indicating the OS the builder requires for
      the machines that run it. Emits a dimension of the form 'os:os'. By
      default considered None.
    * builderless - a boolean indicating whether the builder runs on builderless
      machines. If True, emits a 'builderless:1' dimension. By default,
      considered True iff `os` refers to a linux OS.
    * auto_builder_dimension - a boolean indicating whether the builder runs on
      machines devoted to the builder. If True, a dimension will be emitted of
      the form 'builder:<name>'. By default, considered True iff `builderless`
      is considered False.
    * mastername - a string with the mastername of the builder. Emits a property
      of the form 'mastername:<mastername>'. By default, considered None.
    * cores - an int indicating the number of cores the builder requires for the
      machines that run it. Emits a dimension of the form 'cores:<cores>' will
      be emitted. By default, considered None.
    * cpu - a member of the `cpu` enum indicating the cpu the builder requires
      for the machines that run it. Emits a dimension of the form 'cpu:<cpu>'.
      By default, considered None.
    * pool - a string indicating the pool of the machines that run the builder.
      Emits a dimension of the form 'pool:<pool>'. By default, considered None.
      When running a builder that has no explicit pool dimension, buildbucket
      inserts one of the form 'pool:luci.<project>.<bucket>'.
    * ssd - a boolean indicating whether the builder runs on machines with ssd.
      If True, emits a 'ssd:1' dimension. If False, emits a 'ssd:0' parameter.
      By default, considered False if builderless is considered True and
      otherwise None.
    * bucketed_triggers - a boolean indicating whether jobs triggered by the
      builder being defined should have the bucket prepended to the builder name
      to trigger. If True, the 'bucketed_triggers' field will be set in the
      '$build/chromium_tests' property. By default, considered False.
    * configure_kitchen - a boolean indicating whether to configure kitchen. If
      True, emits a property to set the 'git_auth' and 'devshell' fields of the
      '$kitchen' property. By default, considered False.
    * goma_backend - a member of the `goma.backend` enum indicating the goma
      backend the builder should use. Will be incorporated into the
      '$build/goma' property. By default, considered None.
    * goma_debug - a boolean indicating whether goma should be debugged. If
      True, the 'debug' field will be set in the '$build/goma' property. By
      default, considered False.
    * goma_enable_ats - a boolean indicating whether ats should be enabled for
      goma. If True, the 'enable_ats' field will be set in the '$build/goma'
      property. By default, considered False.
    * goma_jobs - a member of the `goma.jobs` enum indicating the number of jobs
      to be used by the builder. Sets the 'jobs' field of the '$build/goma'
      property will be set according to the enum member. By default, the 'jobs'
      considered None.
    * use_clang_coverage - a boolean indicating whether clang coverage should be
      used. If True, the 'use_clang_coverage" field will be set in the
      '$build/code_coverage' property. By default, considered False.
    * use_java_coverage - a boolean indicating whether java coverage should be
      used. If True, the 'use_java_coverage" field will be set in the
      '$build/code_coverage' property. By default, considered False.
    * should_exonerate_flaky_failures - a boolean indicathing whether the
      builder should exonerate a test failure if it's known to be flaky on ToT.
    * kwargs - Additional keyword arguments to forward on to `luci.builder`.
  """
  # We don't have any need of an explicit dimensions dict,
  # instead we have individual arguments for dimensions
  if 'dimensions' in 'kwargs':
    fail('Explicit dimensions are not supported: '
         + 'use builderless, cores, cpu, os or ssd instead')

  dimensions = {}

  properties = kwargs.pop('properties', {})
  if '$kitchen' in properties:
    fail('Setting "$kitchen" property is not supported: '
         + 'use configure_kitchen instead')
  if '$build/goma' in properties:
    fail('Setting "$build/goma" property is not supported: '
         + 'use goma_backend, goma_dbug, goma_enable_ats and goma_jobs instead')
  if '$build/code_coverage' in properties:
    fail('Setting "$build/code_coverage" property is not supported: '
         + 'use use_clang_coverage and use_java_coverage instead')
  properties = dict(properties)

  os = defaults.get_value('os', os)
  if os:
    dimensions['os'] = os.dimension

  builderless = defaults.get_value('builderless', builderless)
  if builderless == args.COMPUTE:
    builderless = os != None and os.category in _DEFAULT_BUILDERLESS_OS_CATEGORIES
  if builderless:
    dimensions['builderless'] = '1'

  auto_builder_dimension = defaults.get_value(
      'auto_builder_dimension', auto_builder_dimension)
  if auto_builder_dimension == args.COMPUTE:
    auto_builder_dimension = builderless == False
  if auto_builder_dimension:
    dimensions['builder'] = name

  cores = defaults.get_value('cores', cores)
  if cores != None:
    dimensions['cores'] = str(cores)

  cpu = defaults.get_value('cpu', cpu)
  if cpu != None:
    dimensions['cpu'] = cpu

  mastername = defaults.get_value('mastername', mastername)
  if mastername != None:
    properties['mastername'] = mastername

  pool = defaults.get_value('pool', pool)
  if pool:
    dimensions['pool'] = pool

  ssd = defaults.get_value('ssd', ssd)
  if ssd == args.COMPUTE:
    ssd = False if builderless else None
  if ssd != None:
    dimensions['ssd'] = str(int(ssd))

  configure_kitchen = defaults.get_value('configure_kitchen', configure_kitchen)
  if configure_kitchen:
    properties['$kitchen'] = {
        'devshell': True,
        'git_auth': True,
    }

  chromium_tests = _chromium_tests_property(
      bucketed_triggers = bucketed_triggers,
  )
  if chromium_tests != None:
    properties['$build/chromium_tests'] = chromium_tests

  goma = _goma_property(
      goma_backend = goma_backend,
      goma_debug = goma_debug,
      goma_enable_ats = goma_enable_ats,
      goma_jobs = goma_jobs,
      os = os,
  )
  if goma != None:
    properties['$build/goma'] = goma

  code_coverage = _code_coverage_property(
      use_clang_coverage = use_clang_coverage,
      use_java_coverage = use_java_coverage,
  )
  if code_coverage != None:
    properties['$build/code_coverage'] = code_coverage

  should_exonerate_flaky_failures = defaults.get_value(
      'should_exonerate_flaky_failures', should_exonerate_flaky_failures)
  if should_exonerate_flaky_failures:
    properties['$build/test_utils'] = {
      'should_exonerate_flaky_failures': True,
    }

  kwargs = dict(kwargs)
  bucket = defaults.get_value('bucket', bucket)
  if bucket != args.COMPUTE:
    kwargs['bucket'] = bucket
  executable = defaults.get_value('executable', executable)
  if executable != args.COMPUTE:
    kwargs['executable'] = executable
  triggered_by = defaults.get_value('triggered_by', triggered_by)
  if triggered_by != args.COMPUTE:
    kwargs['triggered_by'] = triggered_by

  return luci.builder(
      name = name,
      dimensions = dimensions,
      properties = properties,
      **kwargs
  )


def builder_name(builder, bucket=args.DEFAULT):
  bucket = defaults.get_value('bucket', bucket)
  if bucket == args.COMPUTE:
    fail('Either a default for bucket must be set or bucket must be passed in')
  return '{}/{}'.format(bucket, builder)


builders = struct(
    builder = builder,
    builder_name = builder_name,
    cpu = cpu,
    defaults = defaults,
    goma = goma,
    os = os,
    xcode_cache = xcode_cache,
)
