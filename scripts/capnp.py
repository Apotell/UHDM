import ctypes
import os
import platform
import subprocess
import sys

import config
import file_utils


def _get_schema(type, vpi, card):
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

    type = mapping.get(type, type)
    if card == '1':
        return (vpi, type)
    elif card == 'any':
        return (vpi, f'List({type})')
    else:
        return (None, None)


def generate(models):
    model_schemas = [
      'struct Any {',
      '  uhdmId @0 : UInt32;',
      '  vpiParent @1 : ObjIndexType;',
      '  vpiFile @2 : UInt32;',
      '  vpiLineNo @3 : UInt32;',
      '  vpiEndLineNo @4 : UInt32;',
      '  vpiColumnNo @5 : UInt16;',
      '  vpiEndColumnNo @6 : UInt16;',
      '}',
      ''
    ]

    classnames = set()
    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        basename = model.get('extends', 'Any') or 'Any'

        Classname = classname[:1].upper() + classname[1:].replace('_', '')
        Basename = basename[:1].upper() + basename[1:].replace('_', '')

        if modeltype != 'class_def':
            classnames.add(Classname)

        field_index = 1
        model_schemas.append(f'struct {Classname} {{')
        model_schemas.append(f'  base @0: {Basename};')

        for key, value in model.allitems():
            if key == 'property':
                if value.get('name') != 'type':
                    vpi = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    field_name, field_type = _get_schema(type, vpi, card)
                    if field_name and field_type:
                        model_schemas.append(f'  {field_name} @{field_index} : {field_type};')
                        field_index += 1

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                name = name.replace('_', '')

                obj_key = 'ObjIndexType' if key in ['class_ref', 'group_ref'] else 'UInt64'

                field_name, field_type = _get_schema(obj_key, name, card)
                if field_name and field_type:
                    model_schemas.append(f'  {field_name} @{field_index} : {field_type};')
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
