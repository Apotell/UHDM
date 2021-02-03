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


import config
import file_utils
from collections import OrderedDict


def generate(models):
    types = OrderedDict()
    for model in models.values():
        for key, value in model.items():
            if key == 'property':
                for prop, conf in value.items():
                    type = conf.get('type')
                    card = conf.get('card')

                    if (prop != 'type') and (type != 'any') and (card == 'any'):
                        types[type] = None

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                for name, refs in value.items():
                    if refs:
                        card = refs.get('card')

                        if card == 'any':
                            type = refs.get('type')
                            if key == 'group_ref':
                                type = 'any'

                            types[type] = None

    containers = []
    for type in types.keys():
        if type != 'any':
            containers.append(f'  class {type};')
        containers.append(f'  typedef std::vector<{type}*> VectorOf{type};')
        containers.append(f'  typedef std::vector<{type}*>::iterator VectorOf{type}Itr;')
    containers = '\n'.join(containers)

    with open(config.get_template_filepath('containers.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CONTAINERS>', containers)
    file_utils.set_content_if_changed(config.get_header_filepath('containers.h'), file_content)
    return True


def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
