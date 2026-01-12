import config
import file_utils


def generate(models):
    TypeNames = []
    ClassNames = []
    for model in models.values():
      model_type = model['type']

      if model_type not in ['group_def']:
        classname = model['name']
        TypeNames.append(config.make_class_name(classname))

      if model_type not in ['class_def', 'group_def']:
        classname = model['name']
        ClassNames.append(config.make_class_name(classname))

    TypeNames = sorted(TypeNames)
    ClassNames = sorted(ClassNames)

    any_functions = [
      (f'  void visit{ClassName}(const uhdm::{ClassName}* object) override {{ '
      f'PYBIND11_OVERRIDE(void, uhdm::UhdmVisitor, visit{ClassName}, object); '
      f'}}') for ClassName in ClassNames
    ]
    many_functions = [
      (f'  void visit{TypeName}Collection(const uhdm::Any* object, const uhdm::{TypeName}Collection& objects) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::UhdmVisitor, visit{TypeName}Collection, object, objects); '
       f'}}') for TypeName in TypeNames
    ]

    any_defs = [f'      .def("visit{ClassName}", &uhdm::UhdmVisitor::visit{ClassName})' for ClassName in ClassNames]
    many_defs = [f'      .def("visit{TypeName}Collection", &uhdm::UhdmVisitor::visit{TypeName}Collection)' for TypeName in TypeNames]

    # py_UhdmVisitor.cpp
    with open(config.get_template_filepath('py_UhdmVisitor.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <VISIT_ANY_FUNCTIONS>', '\n'.join(any_functions).rstrip())
    file_content = file_content.replace('// <VISIT_MANY_FUNCTIONS>', '\n'.join(many_functions).rstrip())
    file_content = file_content.replace('// <VISIT_ANY_DEFS>', '\n'.join(any_defs).rstrip())
    file_content = file_content.replace('// <VISIT_MANY_DEFS>', '\n'.join(many_defs).rstrip())
    file_utils.set_content_if_changed(config.get_output_python_source_filepath('py_UhdmVisitor.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
