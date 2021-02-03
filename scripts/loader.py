# Copyright 2019-2020 Alain Dargelas
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import re
from collections import OrderedDict

import config


def _load_one_model(filepath):
    lineNo = 0
    top_def = None
    cur_def = None
    with open(filepath, 'r+t') as strm:
        for line in strm:
            lineNo += 1

            # Strip out any comment
            pos = line.find('#')
            if pos >= 0:
                line = line[:pos]

            line = line.strip()
            if not line: # empty line, ignore!
                continue

            m = re.match('^[-]*\s*(?P<type>\w+)\s*:\s*(?P<name>.+)$', line)
            if not m:
              print(f'Failed to parse {modeldef_filename}:{lineNo}')
              continue  # TODO(HS): This should be an error!

            type = m.group('type').strip()
            name = m.group('name').strip()
            if type in [ 'obj_def', 'class_def', 'group_def' ]:
                top_def = OrderedDict([
                  ('name', name),
                  ('type', type),
                  ('extends', None),
                  ('card', None),
                  ('property', OrderedDict()),
                  ('class', OrderedDict()),
                  ('class_ref', OrderedDict()),
                  ('obj_ref', OrderedDict()),
                  ('group_ref', OrderedDict()),
                  ('subclasses', set()),
                ])
                cur_def = top_def

            elif type == 'extends':
                top_def['extends'] = name

            elif type in ['property', 'class_ref', 'obj_ref', 'group_ref', 'class']:
                if name not in top_def[type]:
                    top_def[type][name] = OrderedDict()
                cur_def = top_def[type][name]

            elif type in ['type', 'card', 'vpi', 'vpi_obj', 'name']:
                cur_def[type] = name

            else:
                print(f'Unknown type {type}')

    return top_def


def load_models():
    list_filepath = config.get_modellist_filepath()
    base_dirpath = os.path.dirname(list_filepath)

    models = OrderedDict()
    with open(list_filepath, 'r+t') as strm:
        for model_filename in strm:
            pos = model_filename.find('#')
            if pos >= 0:
                model_filename = model_filename[:pos]
            model_filename = model_filename.strip()

            if model_filename:
                model_filepath = os.path.join(base_dirpath, model_filename)
                model = _load_one_model(model_filepath)
                models[model['name']] = model

    # Populate the subclass list
    for name, value in models.items():
        baseclass = value['extends']
        while baseclass:
            models[baseclass]['subclasses'].add(name)
            baseclass = models[baseclass]['extends']

    return models


def _main():
  config.set_cwd()
  load_models()
  return 0


if __name__ == '__main__':
    import sys
    sys.exit(_main())
