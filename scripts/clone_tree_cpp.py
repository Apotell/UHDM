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


def generate(models):
    implementation = []

    for model in models.values():
        modeltype = model.get('type')
        if modeltype != 'obj_def':
            continue

        classname = model.get('name')
        Classname = classname[0].upper() + classname[1:]

        vpi_name = config.make_vpi_name(classname)

        implementation.append(f'{classname}* {classname}::DeepClone(Serializer* serializer, ElaboratorListener* elaborator, BaseClass* parent) const {{')
        if 'Net' in vpi_name:
            implementation.append(f'  {classname}* clone = dynamic_cast<{classname}*>(elaborator->bindNet(VpiName()));')
            implementation.append( '  if (clone == nullptr) {')
            implementation.append(f'    clone = serializer->Make{Classname}();')
            implementation.append( '  }')

        elif 'Parameter' in vpi_name:
            implementation.append(f'  {classname}* clone = dynamic_cast<{classname}*>(elaborator->bindParam(VpiName()));')
            implementation.append( '  if (clone == nullptr) {')
            implementation.append(f'    clone = serializer->Make{Classname}();')
            implementation.append( '  }')

        else:
            implementation.append(f'  {classname}* const clone = serializer->Make{Classname}();')

        implementation.append('  const unsigned long id = clone->UhdmId();')
        implementation.append('  *clone = *this;')
        implementation.append('  clone->UhdmId(id);')

        if classname != 'part_select':
            implementation.append('  clone->VpiParent(parent);')
        else:
            implementation.append('  if (VpiParent()) {')
            implementation.append('    ref_obj* ref = serializer->MakeRef_obj();')
            implementation.append('    clone->VpiParent(ref);')
            implementation.append('    ref->VpiName(VpiParent()->VpiName());')
            implementation.append('    ref->VpiParent(parent);')
            implementation.append('  }')

        baseclass = classname
        while baseclass:
            model = models[baseclass]
            classname = model.get('name')

            for key, value in model.items():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    for name, content in value.items():
                        if not content:
                          continue

                        vpi = content.get('vpi')
                        type = content.get('type')
                        card = content.get('card')

                        cast = 'any' if key == 'group_ref' else type
                        Cast = cast[:1].upper() + cast[1:]
                        method = name[:1].upper() + name[1:]

                        if (card == 'any') and not method.endswith('s'):
                            method += 's'

                        if card == '1':
                            if classname == 'func_call':
                                if method == 'Function':
                                    implementation.append(f'  if (auto obj = {method}()) clone->{method}((function*) obj);')

                            elif classname == 'task_call':
                                if method == 'Task':
                                    implementation.append(f'  if (auto obj = {method}()) clone->{method}((task*) obj);')

                            elif (classname == 'ref_obj') and (method == 'Actual_group'):
                                implementation.append(f'  clone->{method}(elaborator->bindAny(VpiName()));')

                            elif (classname == 'class_defn') and (method == 'Extends'):
                                # prevent loop
                                implementation.append(f'  if (auto obj = {method}()) clone->{method}((extends*)obj);')

                            else:
                                implementation.append(f'  if (auto obj = {method}()) clone->{method}(obj->DeepClone(serializer, elaborator, clone));')

                        else:
                            implementation.append(f'  if (auto vec = {method}()) {{')
                            implementation.append(f'    auto clone_vec = serializer->Make{Cast}Vec();')
                            implementation.append(f'    clone->{method}(clone_vec);')
                            implementation.append( '    for (auto obj : *vec) {')
                            implementation.append( '      clone_vec->push_back(obj->DeepClone(serializer, elaborator, clone));')
                            implementation.append( '    }')
                            implementation.append( '  }')

            baseclass = models[baseclass]['extends']

        implementation.append('  return clone;')
        implementation.append('}')
        implementation.append('')

    with open(config.get_template_filepath('clone_tree.cpp'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CLONE_IMPLEMENTATIONS>', '\n'.join(implementation))
    file_utils.set_content_if_changed(config.get_source_filepath('clone_tree.cpp'), file_content)

    return True

def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
