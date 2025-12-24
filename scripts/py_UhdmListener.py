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
      (f'  void enter{ClassName}(const uhdm::{ClassName}* object, uint32_t vpiRelation) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::UhdmListener, enter{ClassName}, object, vpiRelation); }}\n'
       f'  void leave{ClassName}(const uhdm::{ClassName}* object, uint32_t vpiRelation) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::UhdmListener, leave{ClassName}, object, vpiRelation); }}\n')
      for ClassName in ClassNames
    ]
    many_functions = [
      (f'  void enter{TypeName}Collection(const uhdm::Any* object, const uhdm::{TypeName}Collection& objects, uint32_t vpiRelation) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::UhdmListener, enter{TypeName}Collection, object, objects, vpiRelation); }}\n'
       f'  void leave{TypeName}Collection(const uhdm::Any* object, const uhdm::{TypeName}Collection& objects, uint32_t vpiRelation) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::UhdmListener, leave{TypeName}Collection, object, objects, vpiRelation); }}\n')
      for TypeName in TypeNames
    ]

    any_defs = [
      (f'      .def("enter_{ClassName}", &uhdm::UhdmListener::enter{ClassName})\n'
       f'      .def("leave_{ClassName}", &uhdm::UhdmListener::leave{ClassName})\n')
      for ClassName in ClassNames
    ]
    many_defs = [
      (f'      .def("enter_{TypeName}Collection", &uhdm::UhdmListener::enter{TypeName}Collection)\n'
       f'      .def("leave_{TypeName}Collection", &uhdm::UhdmListener::leave{TypeName}Collection)\n')
      for TypeName in TypeNames
    ]

    # py_UhdmListener.cpp
    with open(config.get_template_filepath('py_UhdmListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <ENTER_LEAVE_ANY_FUNCTIONS>', '\n'.join(any_functions).rstrip())
    file_content = file_content.replace('// <ENTER_LEAVE_MANY_FUNCTIONS>', '\n'.join(many_functions).rstrip())
    file_content = file_content.replace('// <ENTER_LEAVE_ANY_DEFS>', '\n'.join(any_defs).rstrip())
    file_content = file_content.replace('// <ENTER_LEAVE_MANY_DEFS>', '\n'.join(many_defs).rstrip())
    file_utils.set_content_if_changed(config.get_output_python_source_filepath('py_UhdmListener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
