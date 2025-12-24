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

    functions = [
      (f'  void enter{ClassName}(const uhdm::{ClassName}* object, vpiHandle handle) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::VpiListener, enter{ClassName}, object, handle); }}\n'
       f'  void leave{ClassName}(const uhdm::{ClassName}* object, vpiHandle handle) override {{ '
       f'PYBIND11_OVERRIDE(void, uhdm::VpiListener, leave{ClassName}, object, handle); }}\n')
      for ClassName in ClassNames
    ]

    defs = [
      (f'      .def("enter_{ClassName}", &uhdm::VpiListener::enter{ClassName})\n'
       f'      .def("leave_{ClassName}", &uhdm::VpiListener::leave{ClassName})\n')
      for ClassName in ClassNames
    ]

    # py_VpiListener.cpp
    with open(config.get_template_filepath('py_VpiListener.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <ENTER_LEAVE_FUNCTIONS>', '\n'.join(functions).rstrip())
    file_content = file_content.replace('// <ENTER_LEAVE_DEFS>', '\n'.join(defs).rstrip())
    file_utils.set_content_if_changed(config.get_output_python_source_filepath('py_VpiListener.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
