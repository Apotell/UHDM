import config
import file_utils
import uhdm_types_h

def generate(models):
    object_implementations = []
    collection_declarations = []
    collection_implementations = []
    case_statements = []
    case_ignoredVpi_statements = []
    ClassNames = set()
    TypeNames = set()

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        ClassName = config.make_class_name(classname)

        basename = model.get('extends') or 'Any'
        BaseName = config.make_class_name(basename)

        if model.get('subclasses') or modeltype == 'obj_def':
            for key, value in model.allitems():
                if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                    type = value.get('type')
                    card = value.get('card')

                    if key == 'group_ref':
                        type = 'any'

        TypeNames.add(ClassName)
        if not model.get('subclasses'):
            ClassNames.add(ClassName)

    for TypeName in sorted(TypeNames):
        if TypeName in ClassNames:
            object_implementations.append(f'  virtual int32_t compare(const {TypeName}* plhs, const {TypeName}* prhs, uint32_t relation, int32_t r) {{ return r; }}')

            case_statements.append(f'    case UhdmType::{TypeName}: r = compare(static_cast<const {TypeName}*>(lhs), static_cast<const {TypeName}*>(rhs), relation, r); break;')

        collection_declarations.extend([
            f'  virtual int32_t compare(const Any* plhs, const {TypeName}Collection* lhs,',
            f'                          const Any* prhs, const {TypeName}Collection* rhs,',
             '                          uint32_t relation, int32_t r);'
        ])

        collection_implementations.extend([
            f'int32_t UhdmComparer::compare(const Any* plhs, const {TypeName}Collection* lhs,',
            f'                              const Any* prhs, const {TypeName}Collection* rhs,',
             '                              uint32_t relation, int32_t r) {',
            f'  return (r == 0) ? compareT<{TypeName}>(plhs, lhs, prhs, rhs, relation, r) : r;',
             '}',
             ''
        ])

    # UhdmComparer.h
    with open(config.get_template_filepath('UhdmComparer.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <UHDM_COMPARER_OBJECT_IMPLEMENTATIONS>', '\n'.join(object_implementations))
    file_content = file_content.replace('// <UHDM_COMPARER_COLLECTION_DECLARATIONS>', '\n'.join(collection_declarations))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmComparer.h'), file_content)

    # UhdmComparer.cpp
    with open(config.get_template_filepath('UhdmComparer.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <UHDM_COMPARER_COLLECTION_IMPLEMENTATIONS>', '\n'.join(collection_implementations))
    file_content = file_content.replace('// <UHDM_COMPARER_CASE_STATEMENTS>', '\n'.join(case_statements))
    file_utils.set_content_if_changed(config.get_output_source_filepath('UhdmComparer.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
