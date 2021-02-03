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
import platform
import subprocess

import config
import file_utils


def _get_schema(type, vpi, card):
    mapping = {
      'string': 'UInt64',
      'unsigned int': 'UInt64',
      'int': 'Int64',
      'any': 'Int64',
      'bool': 'Bool',
      'value': 'UInt64',
      'delay': 'UInt64',
    }

    type = mapping.get(type, type)
    if card == '1':
        return (vpi, type)
    elif card == 'any':
        return (vpi, f'List({type})')
    else:
        return (None, None)


def generate(models):
    capnp_schema = {}
    capnp_schema_all = []
    capnpRootSchemaIndex = 2
    capnp_root_schema = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        capnp_schema[classname] = []

        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')

        if modeltype != 'class_def':
            capnp_schema[classname].append(('vpiParent', 'UInt64'))
            capnp_schema[classname].append(('uhdmParentType', 'UInt64'))
            capnp_schema[classname].append(('vpiFile', 'UInt64'))
            capnp_schema[classname].append(('vpiLineNo', 'UInt32'))
            capnp_schema[classname].append(('uhdmId', 'UInt64'))

            capnp_root_schema.append(f'  factory{Classname} @{capnpRootSchemaIndex} :List({Classname});')
            capnpRootSchemaIndex += 1

        for key, value in model.items():
            if key == 'property':
                for prop, conf in value.items():
                    if prop != 'type':
                        vpi = conf.get('vpi')
                        type = conf.get('type')
                        card = conf.get('card')
                        capnp_schema[classname].append(_get_schema(type, vpi, card))

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                for name, content in value.items():
                    if not content:
                      continue

                    type = content.get('type')
                    card = content.get('card')

                    if (card == 'any') and not name.endswith('s'):
                        name += 's'

                    name = name.replace('_', '')

                    obj_key = 'ObjIndexType' if key in ['class_ref', 'group_ref'] else 'UInt64'
                    capnp_schema[classname].append(_get_schema(obj_key, name, card))

        if modeltype != 'class_def':
            capnpIndex = 0
            capnp_schema_all.append(f'struct {Classname} {{')
            for name, type in capnp_schema[classname]:
                if name and type:
                    capnp_schema_all.append(f'  {name} @{capnpIndex} :{type};')
                    capnpIndex += 1

            # process baseclass recursively
            baseclass = model['extends']
            while baseclass:
                for name, type in capnp_schema[baseclass]:
                    if name and type:
                        capnp_schema_all.append(f'  {name} @{capnpIndex} :{type};')
                        capnpIndex += 1

                baseclass = models[baseclass]['extends']

            capnp_schema_all.append('}')
            capnp_schema_all.append('')

    with open(config.get_template_filepath('UHDM.capnp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_SCHEMA>', '\n'.join(capnp_schema_all))
    file_content = file_content.replace('<CAPNP_ROOT_SCHEMA>', '\n'.join(capnp_root_schema))

    if file_utils.set_content_if_changed(config.get_source_filepath('UHDM.capnp'), file_content):
        file_utils.remove_file_safely(config.get_source_filepath('UHDM.capnp.h'))
        file_utils.remove_file_safely(config.get_source_filepath('UHDM.capnp.c++'))

        suffix = '.exe' if platform.system() == 'Windows' else ''
        separator = ';' if platform.system() == 'Windows' else ':'
        capnp_plugin_exepath = file_utils.find_file(config.get_cwd(), f'capnpc-c++{suffix}')
        if not capnp_plugin_exepath:
          raise FileNotFoundError('capnp executable was not found')

        capnp_dirpath = os.path.dirname(capnp_plugin_exepath)
        capnp_exepath = os.path.join(capnp_dirpath, f'capnp{suffix}')
        subprocess.check_call(
            f'{capnp_exepath} compile -oc++ UHDM.capnp',
            cwd=config.get_source_dirpath(),
            shell=True,
            stderr=subprocess.STDOUT,
            env=dict(os.environ, PATH=f'{capnp_dirpath}{separator}$PATH'))

    return True


def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
