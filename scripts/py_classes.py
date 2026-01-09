import os

import config
import file_utils


def _get_declarations(class_name, func_name, type, vpi, card, real_type=''):
  content = []
  if type in ['string', 'value', 'delay']:
    type = 'std::string'

  ClassName = config.make_class_name(class_name)
  FuncName = config.make_func_name(func_name, card)
  TypeName = config.make_class_name(type)

  if card == '1':
    if type in ['int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool', 'std::string']:
      content.append(f'      .def_property("{func_name}", &uhdm::{ClassName}::get{FuncName}, &uhdm::{ClassName}::set{FuncName})')

    elif vpi in ['vpiName']:
      content.append(f'      .def_property("name", &uhdm::{ClassName}::getName, &uhdm::{ClassName}::setName)')
      if type in ['identifier']:
        content.extend([
          f'      .def_property("name_obj", [](uhdm::{ClassName}* self) {{ return self->getNameObj(); }},',
          f'                    &uhdm::{ClassName}::setNameObj, pybind11::return_value_policy::reference)',
        ])
    else:
      content.extend([
        f'      .def_property("{func_name}", [](uhdm::{ClassName}* self) {{ return self->get{FuncName}(); }},',
        f'                    &uhdm::{ClassName}::set{FuncName}, pybind11::return_value_policy::reference)',
      ])
  elif card == 'any':
    if not func_name.endswith('s'):
      func_name += 's'

    content.extend([
      f'      .def_property("{func_name}",',
      f'                    [](uhdm::{ClassName}* self) {{ return getCollection(self->get{FuncName}()); }},',
      f'                    [](uhdm::{ClassName}* self, const uhdm::{TypeName}Collection& from) {{ setCollection(from, self->get{FuncName}(true)); }},',
       '                    pybind11::return_value_policy::reference)',
    ])

  return '\n'.join(content)


def _generate_one_class(model, models, template):
  modeltype = model['type']

  classname = model['name']
  ClassName = config.make_class_name(classname)

  basename = model.get('extends') or 'BaseClass'
  BaseName = config.make_class_name(basename)

  declarations = []
  for key, value in model.allitems():
    if key in ['property', 'obj_ref', 'class_ref', 'group_ref']:
      name = value.get('name')
      type = value.get('type')
      vpi = value.get('vpi')
      card = value.get('card')
      real_type = type

      if key == 'group_ref':
          type = 'any'

      declarations.append(_get_declarations(classname, name, type, vpi, card, real_type))

  content = str(template)
  content = content.replace('<classname>', classname)
  content = content.replace('<CLASSNAME>', ClassName)
  content = content.replace('<EXTENDS>', BaseName)
  content = content.replace('// <DEF_DECLARATIONS>', '\n'.join(declarations))

  file_utils.set_content_if_changed(config.get_output_python_source_filepath(f'py_{classname}.cpp'), content)
  return True


def generate(models):
  with open(config.get_template_filepath('py_class_source.cpp'), 'rt') as strm:
    template = strm.read()

  includes = []
  declarations = []
  invocations = []
  for model in models.values():
    modeltype = model['type']

    if modeltype != 'group_def':
      _generate_one_class(model, models, template)

      class_name = model['name']
      ClassName = config.make_class_name(class_name)

      includes.append(f'#include "py_{class_name}.cpp"')
      declarations.append(f'void bind_{ClassName}(pybind11::module_& m);')
      invocations.append(f'  bind_{ClassName}(m);')

  with open(config.get_template_filepath('py_classes.cpp'), 'rt') as strm:
    content = strm.read()

  content = content.replace('// <IMPLEMENTATION_INCLUDES>', '\n'.join(sorted(includes)).rstrip())
  content = content.replace('// <BIND_CLASSES_FORWARD_DECLARATIONS>', '\n'.join(sorted(declarations)).rstrip())
  content = content.replace('// <BIND_CLASSES_INVOCATIONS>', '\n'.join(invocations).rstrip())
  file_utils.set_content_if_changed(config.get_output_python_source_filepath('py_classes.cpp'), content)

  return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
