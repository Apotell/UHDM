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


def _get_iterator(classname, vpi, type, card):
    iterator = []

    if vpi in ['vpiParent', 'vpiInstance']:
        pass # To prevent infinite loops in visitors as these 2 relations are pointing upward in the tree

    elif card == '1':
        # upward vpiModule, vpiInterface relation (when card == 1, pointing to the parent object) creates loops in visitors
        if vpi not in ['vpiModule', 'vpiInterface']:
            iterator.append(f'  itr = vpi_handle({vpi}, object);')
            iterator.append( '  if (itr) {')
            iterator.append(f'    listen_{type}(itr, listener);')
            iterator.append( '    vpi_free_object(itr);')
            iterator.append( '  }')

    else:
        iterator.append(f'  itr = vpi_iterate({vpi}, object);')
        iterator.append( '  if (itr) {')
        iterator.append( '    while (vpiHandle obj = vpi_scan(itr) ) {')
        iterator.append(f'      listen_{type}(obj, listener);')
        iterator.append( '      vpi_free_object(obj);')
        iterator.append( '    }')
        iterator.append( '    vpi_free_object(itr);')
        iterator.append( '  }')

    return iterator


def generate(models):
    declarations = []
    iterators = {}
    listeners = []
    any_listeners = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        Classname_ = classname[:1].upper() + classname[1:]

        iterators[classname] = []
        declarations.append(f'void listen_{classname}(vpiHandle object, UHDM::VpiListener* listener);')

        listeners.append(f'void UHDM::listen_{classname}(vpiHandle object, VpiListener* listener) {{')
        listeners.append(f'  {classname}* d = ({classname}*) ((const uhdm_handle*)object)->object;')
        listeners.append( '  const BaseClass* parent = d->VpiParent();')
        listeners.append( '  vpiHandle parent_h = parent ? NewVpiHandle(parent) : 0;')
        listeners.append(f'  listener->enter{Classname_}(d, parent, object, parent_h);')
        listeners.append( '  vpiHandle itr;')

        any_listeners.append(f'  case uhdm{classname}:')
        any_listeners.append(f'    listen_{classname}(object, listener);')
        any_listeners.append( '    break;')

        for key, value in model.items():
            if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                for name, content in value.items():
                    if not content:
                      continue

                    vpi  = content.get('vpi')
                    type = content.get('type')
                    card = content.get('card')

                    if key == 'group_ref':
                        type = 'any'

                    if not ((key == 'obj_ref') and (vpi == 'vpiFunction') and (card == '1')):
                        listeners.extend(_get_iterator(classname, vpi, type, card))
                        iterators[classname].append((vpi, type, card))

        # process baseclass recursively
        baseclass = model['extends']
        while baseclass:
            for vpi, type, card in iterators[baseclass]:
                listeners.extend(_get_iterator(classname, vpi, type, card))

            baseclass = models[baseclass]['extends']

        listeners.append(f'  listener->leave{Classname_}(d, parent, object, parent_h);')
        listeners.append( '  vpi_release_handle(parent_h);')
        listeners.append( '}')
        listeners.append( '')

    # vpi_listener.h
    with open(config.get_template_filepath('vpi_listener.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENERS_HEADER>', '\n'.join(declarations))
    file_utils.set_content_if_changed(config.get_header_filepath('vpi_listener.h'), file_content)

    # vpi_listener.cpp
    with open(config.get_template_filepath('vpi_listener.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENERS>', '\n'.join(listeners))
    file_content = file_content.replace('<VPI_ANY_LISTENERS>', '\n'.join(any_listeners))
    file_utils.set_content_if_changed(config.get_source_filepath('vpi_listener.cpp'), file_content)

    return True


def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
