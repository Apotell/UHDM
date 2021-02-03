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

import concurrent.futures
import os

import config
import file_utils
import loader

import capnp
import classes_h
import clone_tree_cpp
import containers_h
import ElaboratorListener_h
import serializer
import uhdm_forward_decl_h
import uhdm_h
import uhdm_types_h
import vpi_listener
import vpi_user_cpp
import vpi_visitor_cpp
import VpiListener_h
import VpiListenerTracer_h


def _worker(params):
    key, models = params

    config.log(f' ... {key}')

    if key == 'BaseClass':
        file_utils.copy_file_if_changed(config.get_template_filepath('BaseClass.h'), config.get_header_filepath('BaseClass.h'))
        return True

    elif key == 'capnp':
        return capnp.generate(models)

    elif key == 'classes_h':
        return classes_h.generate(models)

    elif key == 'clone_tree_h':
        file_utils.copy_file_if_changed(config.get_template_filepath('clone_tree.h'), config.get_header_filepath('clone_tree.h'))
        return True

    elif key == 'clone_tree_cpp':
        return clone_tree_cpp.generate(models)

    elif key == 'ElaboratorListener_h':
        return ElaboratorListener_h.generate(models)

    elif key == 'containers_h':
        return containers_h.generate(models)

    elif key == 'serializer':
        return serializer.generate(models)

    elif key == 'SymbolFactory':
        file_utils.copy_file_if_changed(config.get_template_filepath('SymbolFactory.h'), config.get_header_filepath('SymbolFactory.h'))
        file_utils.copy_file_if_changed(config.get_template_filepath('SymbolFactory.cpp'), config.get_source_filepath('SymbolFactory.cpp'))
        return True

    elif key == 'uhdm_forward_decl_h':
        return uhdm_forward_decl_h.generate(models)

    elif key == 'uhdm_h':
        return uhdm_h.generate(models)

    elif key == 'uhdm_types_h':
        return uhdm_types_h.generate(models)

    elif key == 'vpi_listener':
        return vpi_listener.generate(models)

    elif key == 'vpi_uhdm_h':
        file_utils.copy_file_if_changed(config.get_template_filepath('vpi_uhdm.h'), config.get_header_filepath('vpi_uhdm.h'))
        return True

    elif key == 'vpi_user_cpp':
        return vpi_user_cpp.generate(models)

    elif key == 'vpi_visitor_h':
        file_utils.copy_file_if_changed(config.get_template_filepath('vpi_visitor.h'), config.get_header_filepath('vpi_visitor.h'))
        return True

    elif key == 'vpi_visitor_cpp':
        return vpi_visitor_cpp.generate(models)

    elif key == 'VpiListener_h':
        return VpiListener_h.generate(models)

    elif key == 'VpiListenerTracer_h':
        return VpiListenerTracer_h.generate(models)

    config.log('ERROR: Unknown key "{key}"')
    return False


def _main():
    multi = True # Set this to true for single theraded execution
    config.set_cwd()

    print('Generating UHDM models ...')
    print('   Loading models ...')
    models = loader.load_models()
    print(f'   ... found {len(models)} models.')

    print('   Validating ordering ...')
    # Check validity
    index = 0
    order = {}
    for name in models.keys():
        order[name] = index
        index += 1

    for name, model in models.items():
        baseclass = model['extends']
        if baseclass:
            thisIndex = order[name]
            baseIndex = order[baseclass]
            if baseIndex >= thisIndex:
                raise Exception(f'Model {name} should follow {baseclass} in listing.')
    print('   ... all good.')

    params = [
        ('BaseClass', None),
        ('capnp', models),
        ('classes_h', models),
        ('clone_tree_h', None),
        ('clone_tree_cpp', models),
        ('containers_h', models),
        ('ElaboratorListener_h', models),
        ('serializer', models),
        ('SymbolFactory', None),
        ('uhdm_forward_decl_h', models),
        ('uhdm_h', models),
        ('uhdm_types_h', models),
        ('vpi_listener', models),
        ('vpi_uhdm_h', None),
        ('vpi_user_cpp', models),
        ('vpi_visitor_h', None),
        ('vpi_visitor_cpp', models),
        ('VpiListener_h', models),
        ('VpiListenerTracer_h', models),
    ]

    if multi:
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            results = list(executor.map(_worker, params))
    else:
        results = [_worker(args) for args in params]

    result = sum([0 if r else 1 for r in results])
    if result:
        print('ERROR: UHDM model generation FAILED!')
    else:
        print('UHDM Models generated successfully!.')

    return result


if __name__ == '__main__':
    import sys
    sys.exit(_main())
