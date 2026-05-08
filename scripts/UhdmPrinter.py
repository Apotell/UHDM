from collections import OrderedDict

import config
import file_utils


def _get_implementation(name, type, vpi, card):
    content = []
    # TypeName = config.make_class_name(type)
    # FuncName = config.make_func_name(name, card)
    TypeName = type[:1].upper() + type[1:]
    FuncName = name[:1].upper() + name[1:]
    if card == 'any' and not FuncName.endswith('s'):
      FuncName += 's'

    suffix = 'Obj' if type == 'identifier' else ''

    if card == '1':
        content.append(f'  if (const any *const p = object->{FuncName}{suffix}()) printAny(out, p, indent, "{vpi}");')
    else:
        content.append(f'  if (const auto *const p = object->{FuncName}()) print{TypeName}Collection(out, *p, indent, "{vpi}");')

    return content


def _get_vpi_xxx_visitor(name, type, vpi, card):
    content = []
    if vpi in ['vpiType', 'vpiParent', 'vpiFile', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo']:
      return content

    FuncName = vpi[:1].upper() + vpi[1:]
    if card == 'any' and not FuncName.endswith('s'):
      FuncName += 's'

    if (vpi == 'vpiValue') and (type == 'value'):
        content.append('//  s_vpi_value value;')
        content.append('//  vpi_get_value(obj_h, &value);')
        content.append('//  if (value.format) {')
        content.append('//    printProperty(out, "vpiValue", &value, indent);')
        content.append('//    VpiDestroyValue(value);')
        content.append('//  }')
    elif vpi == 'vpiDelay':
        content.append('//  s_vpi_delay delay;')
        content.append('//  vpi_get_delays(obj_h, &delay);')
        content.append('//  if (delay.da != nullptr) {')
        content.append('//    printProperty(out, "vpiDelay", &delay, indent);')
        content.append('//    VpiDestroyDelay(delay);')
        content.append('//  }')
    elif card == '1':
        if type == 'string':
            content.append(f'  if (const std::string_view value = object->{FuncName}(); !value.empty()) printProperty(out, "{vpi}", value, indent);')
        else:
            content.append(f'  if (const auto value = object->{FuncName}()) printProperty(out, "{vpi}", value, indent);')

    return content


def generate(models):
    case_statements = []
    any_declarations = []
    any_implementations = []
    many_declarations = []
    many_implementations = []

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        ClassName = classname[:1].upper() + classname[1:] # config.make_class_name(classname)

        baseclass = model.get('extends', None) or 'Any'
        BaseClass = baseclass[:1].upper() + baseclass[1:] # config.make_class_name(baseclass)

        case_statements.append(f'      case UHDM_OBJECT_TYPE::uhdm{classname}: visit{ClassName}(out, static_cast<const {classname} *>(object), indent); break;')

        any_declarations.append(f'  void visit{ClassName}(std::ostream &out, const {classname} *object, int32_t indent);')
        any_implementations.append(f'void UhdmPrinter::visit{ClassName}(std::ostream &out, const {classname} *object, int32_t indent) {{')
        any_implementations.append(f'  visit{BaseClass}(out, object, indent);')

        many_declarations.append(f'  std::ostream &print{ClassName}Collection(std::ostream &out, const VectorOf{classname} &collection, int32_t indent, std::string_view relation);')
        many_implementations.append((
          f'std::ostream &UhdmPrinter::print{ClassName}Collection(std::ostream &out, const VectorOf{classname} &collection, int32_t indent, std::string_view relation) '
           '{ return printCollection(out, collection, indent, relation); }'
        ))

        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                any_implementations.extend(_get_vpi_xxx_visitor(name, type, vpi, card))

            elif key in ['obj_ref', 'class_ref', 'group_ref', 'class']:
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if key == 'group_ref':
                    type = 'any'

                any_implementations.extend(_get_implementation(name, type, vpi, card))

        any_implementations.append(f'}}')
        any_implementations.append('')

    # UhdmPrinter.h
    with open(config.get_template_filepath('UhdmPrinter.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('//<UHDMPRINTER_VISIT_ANY_DECLARATIONS>', '\n'.join(any_declarations))
    file_content = file_content.replace('//<UHDMPRINTER_PRINT_MANY_DECLARATIONS>', '\n'.join(many_declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmPrinter.h'), file_content)

    # UhdmPrinter.cpp
    with open(config.get_template_filepath('UhdmPrinter.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('//<UHDMPRINTER_VISIT_ANY_IMPLEMENTATIONS>', '\n'.join(any_implementations))
    file_content = file_content.replace('//<UHDMPRINTER_PRINT_MANY_IMPLEMENTATIONS>', '\n'.join(many_implementations))
    file_content = file_content.replace('//<UHDMPRINTER_PRINTANY_CASE_STATEMENTS>', '\n'.join(sorted(case_statements)))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UhdmPrinter.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
