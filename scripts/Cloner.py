import config
import file_utils


def _generate_copy_implementation(model):
  classname = model.get('name')
  modeltype = model.get('type')

  Classname = classname[0].upper() + classname[1:]
  vpi_name = config.make_vpi_name(classname)

  content = [
    f'void Cloner::copy(const {classname}* source, {classname}* target) {{',
    f'  copy(static_cast<const {classname}::basetype_t*>(source), static_cast<{classname}::basetype_t*>(target));',
  ]

  for key, value in model.allitems():
    if key in ['class', 'obj_ref', 'class_ref', 'group_ref', 'property']:
      name = value.get('name')
      card = value.get('card')
      type = value.get('type')
      vpi = value.get('vpi')

      if key == 'property' and type == 'string':
        if vpi not in ['vpiFullName']:
          method = vpi[:1].upper() + vpi[1:]
          content.append(f'  target->{method}(source->{method}());')
      elif key != 'property':
        method = name[:1].upper() + name[1:]
        if card == '1':
          content.append(f'  if (auto obj = source->{method}()) target->{method}(clone(obj, target));')
        else:
          if not method.endswith('s'):
            method += 's'
          content.append(f'  if (auto vec = source->{method}()) target->{method}(clone(vec, target));')

  content.append('}')
  content.append('')

  return content


def generate(models):
  classnames = set([
    model['name'] for model in models.values() if model['type'] == 'obj_def'
  ])

  copy_declarations = []
  copy_implementations = []
  clone_any_declarations = []
  clone_any_implementations = []
  clone_many_declarations = []
  clone_many_implementations = []
  clone_any_case_statements = []

  passthrough_any_declarations = []
  passthrough_many_declarations = []
  passthrough_any_implementations = []
  passthrough_many_implementations = []

  for model in models.values():
    type = model['type']
    classname = model['name']

    Classname = classname[0].upper() + classname[1:]

    return_type = classname
    if classname.endswith('_call') and classname in ['func_call', 'method_func_call', 'method_task_call', 'task_call']:
      return_type = 'tf_call'
    elif (classname.endswith('_typespec') or classname in ['type_parameter']) and classname not in ['ref_typespec']:
      return_type = 'typespec'

    if type == 'obj_def':
      clone_any_declarations.append(f'  virtual {return_type}* clone(const {classname}* source, any* parent);')
      clone_any_implementations.append(f'{return_type}* Cloner::clone(const {classname}* source, any* parent) {{ return cloneT<{classname}>(source, parent); }}')
      clone_any_case_statements.append(f'    case UHDM_OBJECT_TYPE::uhdm{classname}: target = clone(static_cast<const {classname} *>(source), parent); break;')
      passthrough_any_declarations.append(f'  {return_type}* clone(const {classname}* source, any* parent) override;')
      passthrough_any_implementations.append(f'{return_type}* PassThroughCloner::clone(const {classname}* source, any* parent) {{ return cloneT<{classname}>(source, parent); }}')

    if type != 'group_def':
      copy_declarations.append(f'  virtual void copy(const {classname}* source, {classname}* target);')
      copy_implementations.extend(_generate_copy_implementation(model))
      clone_many_declarations.append(f'  virtual VectorOf{classname}* clone(const VectorOf{classname}* source, any* parent);')
      clone_many_implementations.append(f'VectorOf{classname}* Cloner::clone(const VectorOf{classname}* source, any* parent) {{ return cloneT<{classname}>(source, parent); }}')
      passthrough_many_declarations.append(f'  VectorOf{classname}* clone(const VectorOf{classname}* source, any* parent) override;')
      passthrough_many_implementations.append(f'VectorOf{classname}* PassThroughCloner::clone(const VectorOf{classname}* source, any* parent) {{ return cloneT<{classname}>(source, parent); }}')

  # Cloner.h
  with open(config.get_template_filepath('Cloner.h'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<COPY_DECLARATIONS>', '\n'.join(sorted(copy_declarations)))
  file_content = file_content.replace('//<CLONE_ANY_DECLARATIONS>', '\n'.join(sorted(clone_any_declarations)))
  file_content = file_content.replace('//<CLONE_MANY_DECLARATIONS>', '\n'.join(sorted(clone_many_declarations)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_ANY_DECLARATIONS>', '\n'.join(sorted(passthrough_any_declarations)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_MANY_DECLARATIONS>', '\n'.join(sorted(passthrough_many_declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Cloner.h'), file_content)

  # Cloner.cpp
  with open(config.get_template_filepath('Cloner.cpp'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<COPY_IMPLEMENTATIONS>', '\n'.join(copy_implementations[:-1]))
  file_content = file_content.replace('//<CLONE_ANY_IMPLEMENTATIONS>', '\n'.join(sorted(clone_any_implementations)))
  file_content = file_content.replace('//<CLONE_MANY_IMPLEMENTATIONS>', '\n'.join(sorted(clone_many_implementations)))
  file_content = file_content.replace('//<CLONE_ANY_CASE_STATEMENTS>', '\n'.join(sorted(clone_any_case_statements)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_ANY_IMPLEMENTATIONS>', '\n'.join(sorted(passthrough_any_implementations)))
  file_content = file_content.replace('//<PASSTHROUGHCLONER_MANY_IMPLEMENTATIONS>', '\n'.join(sorted(passthrough_many_implementations)))
  file_utils.set_content_if_changed(config.get_output_source_filepath('Cloner.cpp'), file_content)

  return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
