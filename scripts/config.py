import os
from threading import Thread, Lock

_source_dirpath = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
_output_dirpath = _source_dirpath

_include_dirname = 'include'
_models_dirname = 'model'
_templates_dirname = 'templates'
_output_headers_dirname = 'uhdm'
_output_sources_dirname = 'src'
_verbose = True

_log_mutex = Lock()


def verbosity():
  global _verbose
  return _verbose


def log(text, end='\n'):
  global _verbose
  if _verbose:
    _log_mutex.acquire()
    try:
      print(text, end=end, flush=True)
    finally:
      _log_mutex.release()


def configure(args=None):
  global _source_dirpath
  global _output_dirpath

  if args:
    if args.source_dirpath:
      _source_dirpath = args.source_dirpath

    if args.output_dirpath:
      _output_dirpath = args.output_dirpath


  output_headers_dirpath = os.path.join(_output_dirpath, _output_headers_dirname)
  if not os.path.exists(output_headers_dirpath):
    os.makedirs(output_headers_dirpath)

  output_sources_dirpath = os.path.join(_output_dirpath, _output_sources_dirname)
  if not os.path.exists(output_sources_dirpath):
    os.makedirs(output_sources_dirpath)

  print(f'Configuration update: srcdir={_source_dirpath}, headers={output_headers_dirpath}, sources={output_sources_dirpath}')


def get_source_dirpath():
  global _source_dirpath
  return _source_dirpath


def get_include_dirpath():
  global _source_dirpath
  global _include_dirname
  return os.path.join(_source_dirpath, _include_dirname)

def get_include_filepath(filename):
  global _source_dirpath
  global _include_dirname
  return os.path.join(_source_dirpath, _include_dirname, filename)


def get_template_dirpath():
  global _source_dirpath
  global _templates_dirname
  return os.path.join(_source_dirpath, _templates_dirname)


def get_template_filepath(filename):
  global _source_dirpath
  global _templates_dirname
  return os.path.join(_source_dirpath, _templates_dirname, filename)


def get_output_header_dirpath():
  global _output_dirpath
  global _output_headers_dirname
  return os.path.join(_output_dirpath, _output_headers_dirname)


def get_output_source_dirpath():
  global _output_dirpath
  global _output_sources_dirname
  return os.path.join(_output_dirpath, _output_sources_dirname)


def get_output_header_filepath(filename):
  global _output_dirpath
  global _output_headers_dirname
  return os.path.join(_output_dirpath, _output_headers_dirname, filename)


def get_output_source_filepath(filename):
  global _output_dirpath
  global _output_sources_dirname
  return os.path.join(_output_dirpath, _output_sources_dirname, filename)


def get_modellist_filepath():
  global _source_dirpath
  global _models_dirname
  return os.path.join(_source_dirpath, _models_dirname, 'models.lst')


def _normalize(name: str) -> str:
  underscore = True
  Name = ''
  for ch in name:
    if ch == '_':
      underscore = True
    elif underscore:
      Name += ch.upper()
      underscore = False
    else:
      Name += ch
  return Name


def make_class_name(name: str) -> str:
  overrides = {
    'AliasStmt': 'Alias',
    'AssertStmt': 'Assert',
    'ClockingIoDecl': 'ClockingIODecl',
    'InterfaceInst': 'Interface',
    'InterfaceTfDecl': 'InterfaceTFDecl',
    'IoDecl': 'IODecl',
    'ProcessStmt': 'Process',
    'TfCall': 'TFCall',
    'ThreadObj': 'Thread',
  }
  name = _normalize(name)
  return overrides.get(name, name)


def make_func_name(name: str, card: str) -> str:
  overrides = {
    'AliasStmt': 'Alias',
    'AliasStmts': 'Aliases',
    'AssertStmt': 'Assert',
    'AssertStmts': 'Asserts',
    'ClockingIoDecl': 'ClockingIODecl',
    'ClockingIoDecls': 'ClockingIODecls',
    'InterfaceTfDecl': 'InterfaceTFDecl',
    'InterfaceTfDecls': 'InterfaceTFDecls',
    'IoDecl': 'IODecl',
    'IoDecls': 'IODecls',
    'ProcessStmt': 'Process',
    'ProcessStmts': 'Processes',
    'TableEntrys': 'TableEntries',
    'TfCallArgs': 'TFCallArgs',
    'ThreadObj': 'Thread',
    'TfCall': 'TFCall',
    'Indexs': 'Indexes',
    'DpiPure': 'DPIPure',
    'DpiContext': 'DPIContext',
    'DpiCstr': 'DPICStr',
    'DpiCIdentifier': 'DPICIdentifier',
  }
  name = _normalize(name)
  name = overrides.get(name, name)
  if card == 'any' and not name.endswith('s'):
    name += 's'
  name = overrides.get(name, name)
  return name

def make_var_name(name: str, card: str) -> str:
  overrides = {
    'AliasStmt': 'alias',
    'AliasStmts': 'aliases',
    'AssertStmt': 'assert',
    'AssertStmts': 'asserts',
    'ClockingIoDecl': 'clockingIODecl',
    'ClockingIoDecls': 'clockingIODecls',
    'InterfaceTfDecl': 'interfaceTFDecl',
    'InterfaceTfDecls': 'interfaceTFDecls',
    'IoDecl': 'iODecl',
    'IoDecls': 'iODecls',
    'ProcessStmt': 'process',
    'ProcessStmts': 'processes',
    'TableEntrys': 'tableEntries',
    'TfCallArgs': 'tFCallArgs',
    'ThreadObj': 'thread',
    'Indexs': 'indexes',
    'DpiPure': 'dPIPure',
    'DpiContext': 'dPIContext',
    'DpiCstr': 'dPICStr',
    'DpiCIdentifier': 'dPICIdentifier',
  }
  name = _normalize(name)
  name = overrides.get(name, name)
  if card == 'any' and not name.endswith('s'):
    name += 's'
  name = overrides.get(name, name)
  name = name[0].lower() + name[1:]
  return name

def make_vpi_name(name):
  overrides = {
    'vpiForkStmt': 'vpiFork',
    'vpiForStmt': 'vpiFor',
    'vpiIoDecl': 'vpiIODecl',
    'vpiClockingIoDecl': 'vpiClockingIODecl',
    'vpiTfCall': 'vpiSysTfCall',
    'vpiAtomicStmt': 'vpiStmt',
    'vpiAssertStmt': 'vpiAssert',
    'vpiClockedProperty': 'vpiClockedProp',
    'vpiIfStmt': 'vpiIf',
    'vpiWhileStmt': 'vpiWhile',
    'vpiCaseStmt': 'vpiCase',
    'vpiContinueStmt': 'vpiContinue',
    'vpiBreakStmt': 'vpiBreak',
    'vpiReturnStmt': 'vpiReturn',
    'vpiProcessStmt': 'vpiProcess',
    'vpiForeverStmt': 'vpiForever',
    'vpiConstrForeach': 'vpiConstrForEach',
    'vpiFinalStmt': 'vpiFinal',
    'vpiWaitStmt': 'vpiWait',
    'vpiThreadObj': 'vpiThread',
    'vpiSwitchTran': 'vpiSwitch',
  }

  vpi_name = f'vpi{_normalize(name)}'
  return overrides.get(vpi_name, vpi_name)
