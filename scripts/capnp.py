import ctypes
import os
import platform
import subprocess
import sys

import config
import file_utils


def _get_schema(name, type, card):
    mapping = {
      'string': 'UInt32',
      'unsigned': 'UInt32',
      'unsigned int': 'UInt32',
      'uint16_t': 'UInt16',
      'uint32_t': 'UInt32',
      'int': 'Int32',
      'int16_t': 'Int16',
      'int32_t': 'Int32',
      'any': 'Int64',
      'bool': 'Bool',
      'value': 'UInt64',
      'delay': 'UInt64',
      'symbol': 'UInt32',
    }

    name = config.make_var_name(name, card)
    type = mapping.get(type, type)

    if card == '1':
        return (name, type)
    elif card == 'any':
        return (name, f'List({type})')
    else:
        return (None, None)


def generate(models):
    model_schemas = [
      'struct Any {',
      '  uhdmId @0 : UInt32;',
      '  parent @1 : ObjIndexType;',
      '  file @2 : UInt32;',
      '  startLine @3 : UInt32;',
      '  endLine @4 : UInt32;',
      '  startColumn @5 : UInt16;',
      '  endColumn @6 : UInt16;',
      '}',
      ''
    ]

    classnames = set()
    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        basename = model.get('extends') or 'Any'

        ClassName = config.make_class_name(classname)
        BaseName = config.make_class_name(basename)

        if modeltype != 'class_def':
            classnames.add(ClassName)

        field_index = 1
        model_schemas.append(f'struct {ClassName} {{')
        model_schemas.append(f'  base @0: {BaseName};')

        for key, value in model.allitems():
            if key == 'property':
                if value.get('name') != 'type':
                    name = value.get('name')
                    type = value.get('type')
                    card = value.get('card')

                    field_name, field_type = _get_schema(name, type, card)
                    if field_name and field_type:
                        model_schemas.append(f'  {field_name} @{field_index}: {field_type};')
                        field_index += 1

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                name = config.make_var_name(name, card)
                obj_key = 'ObjIndexType' if key in ['class_ref', 'group_ref'] else 'UInt64'

                field_name, field_type = _get_schema(name, obj_key, card)
                if field_name and field_type:
                    model_schemas.append(f'  {field_name} @{field_index}: {field_type};')
                    field_index += 1

        model_schemas.append('}')
        model_schemas.append('')

    root_schema = []
    root_schema_index = 3
    for Classname in sorted(classnames):
        root_schema.append(f'  factory{Classname} @{root_schema_index} : List({Classname});')
        root_schema_index += 1

    with open(config.get_template_filepath('UHDM.capnp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_SCHEMA>', '\n'.join(model_schemas))
    file_content = file_content.replace('<CAPNP_ROOT_SCHEMA>', '\n'.join(root_schema))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UHDM.capnp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
