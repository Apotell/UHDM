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
    vpi_listener = []

    for model in models.values():
        modeltype = model.get('type')
        if modeltype != 'obj_def':
            continue

        classname = model.get('name')
        if classname != 'module':
            continue

        Classname = classname[0].upper() + classname[1:]
        vpi_name = config.make_vpi_name(classname)

        baseclass = classname
        while baseclass:
            model = models[baseclass]

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
                            vpi_listener.append(f'          if (auto obj = defMod->{method}()) {{')
                            vpi_listener.append( '            auto* stmt = obj->DeepClone(serializer_, this, defMod);')
                            vpi_listener.append( '            stmt->VpiParent(inst);')
                            vpi_listener.append(f'            inst->{method}(stmt);')
                            vpi_listener.append( '          }')

                        elif method == 'Task_funcs':
                            vpi_listener.append(f'          if (auto vec = defMod->{method}()) {{')
                            vpi_listener.append(f'            auto clone_vec = serializer_->Make{Cast}Vec();')
                            vpi_listener.append(f'            inst->{method}(clone_vec);')
                            vpi_listener.append( '            for (auto obj : *vec) {')
                            vpi_listener.append( '              clone_vec->push_back((task_func*) obj);')
                            vpi_listener.append( '            }')
                            vpi_listener.append( '          }')

                        elif method not in ['Ports', 'Nets']:
                            # We don't want to override the elaborated instance ports by the module def ports
                            vpi_listener.append(f'          if (auto vec = defMod->{method}()) {{')
                            vpi_listener.append(f'            auto clone_vec = serializer_->Make{Cast}Vec();')
                            vpi_listener.append(f'            inst->{method}(clone_vec);')
                            vpi_listener.append( '            for (auto obj : *vec) {')
                            vpi_listener.append( '              auto* stmt = obj->DeepClone(serializer_, this, defMod);')
                            vpi_listener.append( '              stmt->VpiParent(inst);')
                            vpi_listener.append( '              clone_vec->push_back(stmt);')
                            vpi_listener.append( '            }')
                            vpi_listener.append( '          }')

            baseclass = models[baseclass]['extends']

    with open(config.get_template_filepath('ElaboratorListener.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<ELABORATOR_LISTENER>', '\n'.join(vpi_listener))
    file_utils.set_content_if_changed(config.get_header_filepath('ElaboratorListener.h'), file_content)

    return True

def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
