import orderedmultidict
import os
import pprint
import re
from collections import OrderedDict

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

def _load_one_model(filepath):
    lineNo = 0
    name = ''
    with open(filepath, 'rt') as strm:
        for line in strm:
            lineNo += 1
            line = line.rstrip()

            if not line.strip():
                continue

            m = re.match('^\s*[-]?\s*(?P<key>\w+)\s*:\s*(?P<value>.+)$', line)
            if not m:
                continue  # TODO(HS): Should this be an error?

            key = m.group('key').strip()
            value = m.group('value').strip()

            if key in [ 'property','class_ref', 'obj_ref', 'group_ref' ]:
                name = _normalize(value)

            elif key == 'vpi':
                if not value.startswith('vpi'):
                    print(f'Misnamed vpi name: {filepath}:{lineNo}')
                else:
                    value = value[3:]
                    if value != name:
                        print(f'Mismatch name/vpi: {filepath}:{lineNo}, {name}, {value}')


def load_models():
    list_filepath = r"E:\Davenche\UHDM\model\models.lst"
    base_dirpath = os.path.dirname(list_filepath)

    with open(list_filepath, 'rt') as strm:
        for model_filename in strm:
            pos = model_filename.find('#')
            if pos >= 0:
                model_filename = model_filename[:pos]
            model_filename = model_filename.strip()

            if model_filename:
                model_filepath = os.path.join(base_dirpath, model_filename)
                _load_one_model(model_filepath)

                # if config.verbosity():
                #     print(f"{'=' * 40} {model_filename} {'=' * 40}")
                #     pprint.pprint(model)
                #     print('')


def _main():
  load_models()

  return True


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
