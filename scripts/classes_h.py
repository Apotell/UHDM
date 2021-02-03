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
from collections import OrderedDict

import config
import file_utils


def _print_methods(classname, type, vpi, card, real_type=''):
    content = []
    if type in ['string', 'value', 'delay']:
        type = 'std::string'
    if vpi == 'uhdmType':
        type = 'UHDM_OBJECT_TYPE'

    final = ''
    virtual = ''
    if vpi in ['vpiParent', 'uhdmParentType', 'uhdmType', 'vpiLineNo', 'vpiFile']:
        final = ' final'
        virtual = 'virtual '

    check = ''
    if type == 'any':
        check = f'if (!{real_type}GroupCompliant(data)) return false;'

    Vpi_ = vpi[:1].upper() + vpi[1:]

    if card == '1':
        pointer = ''
        const = ''
        if type not in ['unsigned int', 'int', 'bool', 'std::string']:
            pointer = '*'
            const = 'const '

        if type == 'std::string':
            content.append(f'    {virtual}bool {Vpi_}(const {type}{pointer}& data){final};')
            if vpi == 'vpiFullName':
                content.append(f'    {virtual}const {type}{pointer}& {Vpi_}() const{final};')
            else:
                content.append(f'    {virtual}const {type}{pointer}& {Vpi_}() const{final};')
        else:
            content.append(f'    {virtual}{const}{type}{pointer} {Vpi_}() const {final} {{ return {vpi}_; }}')
            if vpi == 'vpiParent':
                content.append(f'    virtual bool {Vpi_}({type}{pointer} data) final {{ {check} {vpi}_ = data; if (data) uhdmParentType_ = data->UhdmType(); return true; }}')
            else:
                content.append(f'    {virtual}bool {Vpi_}({type}{pointer} data) {final} {{ {check} {vpi}_ = data; return true; }}')
    elif card == 'any':
        content.append(f'    VectorOf{type}* {Vpi_}() const {{ return {vpi}_; }}')
        content.append(f'    bool {Vpi_}(VectorOf{type}* data) {{ {check} {vpi}_ = data; return true; }}')

    return content


def _print_members(type, vpi, card):
    content = []

    if type in ['string', 'value', 'delay']:
        type = 'std::string'

    if card == '1':
        pointer = ''
        default_assignment = '0'
        if type not in ['unsigned int', 'int', 'bool', 'std::string']:
            pointer = '*'
            default_assignment = 'nullptr'

        if type == 'std::string':
            content.append(f'    SymbolFactory::ID {vpi}_ = 0;')
        else:
            content.append(f'    {type}{pointer} {vpi}_ = {default_assignment};')

    elif card == 'any':
        content.append(f'    VectorOf{type}* {vpi}_ = nullptr;')

    return content


members = {}
def _generate_group_checker(model, models, templates):
    groupname = model.get('name')
    modeltype = model.get('type')

    files = {
        'group_header.h': config.get_header_filepath(f'{groupname}.h'),
        'group_header.cpp': config.get_source_filepath(f'{groupname}.cpp'),
    }

    members[groupname] = set()
    for input, output in files.items():
        checktype = set()
        for key, value in model.items():
            if key in ['obj_ref', 'class_ref', 'group_ref'] and value:
                for name, content in value.items():
                    members[groupname].add(name)

                    if key == 'group_ref':
                        if name not in members:
                            print(f'ERROR: Group {name} unknown while processing group {groupname}')
                        else:
                            checktype.update([f'(uhdmtype != uhdm{member})' for member in members[name]])
                    else:
                        checktype.add(f'(uhdmtype != uhdm{name})')

                    if key == 'class_ref':
                        subclasses = models[name]['subclasses']
                        checktype.update([f'(uhdmtype != uhdm{subclass})' for subclass in subclasses])

        prefix = ' ' * 6
        checktype = ' &&\n'.join([ prefix + ct for ct in sorted(checktype) ])

        file_content = templates[input]
        file_content = file_content.replace('<GROUPNAME>', groupname)
        file_content = file_content.replace('<UPPER_GROUPNAME>', groupname.upper())
        file_content = file_content.replace('<CHECKTYPE>', checktype)
        file_utils.set_content_if_changed(output, file_content)

    return True


def _generate_one_class(model, models, templates):
    file_content = templates['class_header.h']
    classname = model['name']
    modeltype = model['type']
    baseclass = model['extends']
    methods = []
    members = []

    Classname_ = classname[:1].upper() + classname[1:]
    Classname = Classname_.replace('_', '')

    if modeltype == 'class_def':
        # DeepClone() not implemented for class_def; just declare to narrow the covariant return type.
        methods.append(f'    {classname}* DeepClone(Serializer* serializer, ElaboratorListener* elab_listener, BaseClass* parent) const override = 0;')
    else:
        # Builtin properties do not need to be specified in each models
        # Builtins: "vpiParent, Parent type, vpiFile, Id" method and field
        methods.extend(_print_methods(classname, 'BaseClass', 'vpiParent', '1'))
        members.extend(_print_members('BaseClass', 'vpiParent', '1'))

        methods.extend(_print_methods(classname, 'unsigned int', 'uhdmParentType', '1'))
        members.extend(_print_members('unsigned int', 'uhdmParentType', '1'))

        methods.extend(_print_methods(classname, 'string','vpiFile', '1'))
        members.extend(_print_members('string', 'vpiFile', '1'))

        methods.extend(_print_methods(classname, 'unsigned int', 'uhdmId', '1'))
        members.extend(_print_members('unsigned int', 'uhdmId', '1'))

        methods.append(f'    {classname}* DeepClone(Serializer* serializer, ElaboratorListener* elab_listener, BaseClass* parent) const override;')

    type_specified = False
    for key, value in model.items():
        if key == 'property':
            for prop, conf in value.items():
                name = conf.get('name')
                vpi = conf.get('vpi')
                type = conf.get('type')
                card = conf.get('card')

                if prop == 'type':
                    type_specified = True
                    methods.append(f'    {type} {vpi[:1].upper() + vpi[1:]}() const final {{ return {name}; }}')
                    continue

                # properties are already defined in vpi_user.h, no need to redefine them
                methods.extend(_print_methods(classname, type, vpi, card))
                members.extend(_print_members(type, vpi, card))

        elif key == 'extends' and value:
            file_content = file_content.replace('<EXTENDS>', value)

        elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
            for name, content in value.items():
                if not content:
                  continue

                vpi  = content.get('vpi')
                type = content.get('type')
                card = content.get('card')
                id   = content.get('id')

                Type_ = type[:1].upper() + type[1:]
                Type = Type_.replace('_', '')
                Name_ = name
                Name = name.replace('_', '')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'
                real_type = type

                if key == 'group_ref':
                    type = 'any'

                methods.extend(_print_methods(classname, type, name, card, real_type))
                members.extend(_print_members(type, name, card))

    if not type_specified and (modeltype == 'obj_def'):
        vpiclasstype = config.make_vpi_name(classname)
        methods.append(f'    unsigned int VpiType() const final {{ return {vpiclasstype}; }}')

    if modeltype == 'class_def':
        file_content = file_content.replace('<FINAL_DESTRUCTOR>', '')
        file_content = file_content.replace('<VIRTUAL>', 'virtual ')
        file_content = file_content.replace('<OVERRIDE_OR_FINAL>', 'override')
        file_content = file_content.replace('<DISABLE_OBJECT_FACTORY>', '#if 0 // This class cannot be instantiated')
        file_content = file_content.replace('<END_DISABLE_OBJECT_FACTORY>', '#endif')
    else:
        file_content = file_content.replace('<FINAL_DESTRUCTOR>', 'final')
        file_content = file_content.replace('<VIRTUAL>', 'virtual ')
        file_content = file_content.replace('<OVERRIDE_OR_FINAL>', 'final')
        file_content = file_content.replace('<DISABLE_OBJECT_FACTORY>', '')
        file_content = file_content.replace('<END_DISABLE_OBJECT_FACTORY>', '')

    file_content = file_content.replace('<EXTENDS>', 'BaseClass')
    file_content = file_content.replace('<CLASSNAME>', classname)
    file_content = file_content.replace('<UPPER_CLASSNAME>', classname.upper())
    file_content = file_content.replace('<METHODS>', '\n\n'.join(methods))
    file_content = file_content.replace('<MEMBERS>', '\n\n'.join(members))

    file_utils.set_content_if_changed(config.get_header_filepath(f'{classname}.h'), file_content)
    return True


def generate(models):
    templates = {}
    for filename in [ 'class_header.h', 'group_header.h', 'group_header.cpp' ]:
        template_filepath = config.get_template_filepath(filename)
        with open(template_filepath, 'r+t') as strm:
            templates[filename] = strm.read()

    for model in models.values():
        classname = model['name']
        modeltype = model['type']

        config.log(f'> Generating headers/{classname}.h')

        if modeltype == 'group_def':
            _generate_group_checker(model, models, templates)
        else:
            _generate_one_class(model, models, templates)

    return True


def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
