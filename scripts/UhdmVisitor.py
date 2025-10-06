import config
import file_utils
import uhdm_types_h


def _get_listen_implementation(classname, name, vpi, type, card):
    content = []

    FuncName = config.make_func_name(name, card)
    TypeName = config.make_class_name(type)

    if card == '1':
        suffix = 'Obj 'if vpi in ['vpiName'] else ''
        content.append(f'  if (const Any *const any = object->get{FuncName}{suffix}()) visit(any);')
    else:
        content.append(f'  if (const {TypeName}Collection *const collection = object->get{FuncName}()) {{')
        content.append(f'    visit{TypeName}Collection(object, *collection);')
        content.append(f'    for ({TypeName}Collection::const_reference any : *collection) visit(any);')
        content.append( '  }')

    return content


def generate(models):
    public_declarations = []
    private_declarations = []
    implementations = []
    ClassNames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        ClassName = config.make_class_name(classname)

        basename = model.get('extends') or 'Any'
        BaseName = config.make_class_name(basename)

        if modeltype == 'obj_def':
            ClassNames.add(ClassName)

            public_declarations.append(f'  virtual void visit{ClassName}(const {ClassName}* object) {{}}')

        if modeltype in ['obj_def', 'class_def']:
            private_declarations.append(f'  void visit{ClassName}_(const {ClassName}* object);')
            implementations.append(f'void UhdmVisitor::visit{ClassName}_(const {ClassName}* object) {{')
            implementations.append(f'  visit{BaseName}_(object);')

            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    name = value.get('name')
                    vpi  = value.get('vpi')
                    type = value.get('type')
                    card = value.get('card')

                    if key == 'group_ref':
                        type = 'any'

                    implementations.extend(_get_listen_implementation(classname, name, vpi, type, card))

            implementations.append( '}')
            implementations.append( '')

    any_case_statements = []
    for ClassName in sorted(ClassNames):
      any_case_statements.append(f'    case UhdmType::{ClassName}: {{')
      any_case_statements.append(f'      visit{ClassName}(static_cast<const {ClassName} *>(object));')
      any_case_statements.append(f'      visit{ClassName}_(static_cast<const {ClassName} *>(object));')
      any_case_statements.append( '    } break;')

    visit_collection_declarations = [
      f'  virtual void visit{TypeName}Collection(const Any* object, const {TypeName}Collection& objects) {{}}'
      for TypeName in sorted(uhdm_types_h.get_type_map(models).keys()) if TypeName != 'BaseClass'
    ]
    public_declarations = sorted(public_declarations)
    private_declarations = sorted(private_declarations)

    # UhdmVisitor.h
    with open(config.get_template_filepath('UhdmVisitor.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <UHDMVISITOR_PUBLIC_VISIT_ANY_DECLARATIONS>', '\n'.join(public_declarations))
    file_content = file_content.replace('// <UHDMVISITOR_PUBLIC_VISIT_COLLECTION_DECLARATIONS>', '\n'.join(visit_collection_declarations))
    file_content = file_content.replace('// <UHDMVISITOR_PRIVATE_VISIT_ANY_DECLARATIONS>', '\n'.join(private_declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmVisitor.h'), file_content)

    # UhdmVisitor.cpp
    with open(config.get_template_filepath('UhdmVisitor.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <UHDMVISITOR_PRIVATE_VISIT_IMPLEMENTATIONS>', '\n'.join(implementations))
    file_content = file_content.replace('// <UHDMVISITOR_VISIT_ANY_CASE_STATEMENTS>', '\n'.join(any_case_statements))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UhdmVisitor.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
