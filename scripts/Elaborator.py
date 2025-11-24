import config
import file_utils


def _generate_module_listeners(models, classname):
  listeners = []
  while classname:
    model = models[classname]

    for key, value in model.allitems():
      if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
        name = value.get('name')
        type = value.get('type')
        card = value.get('card')

        cast = 'any' if key == 'group_ref' else type
        Cast = cast[:1].upper() + cast[1:]
        method = name[:1].upper() + name[1:]

        if (card == 'any') and not method.endswith('s'):
          method += 's'

        if card == '1':
          listeners.append(f'if (auto obj = defMod->{method}()) inst->{method}(clone(obj, inst));')

        elif method in ['Ref_modules', 'Gen_stmts']:
          pass # No elab

        elif method in ['Task_funcs']:
          # We want to deep clone existing instance tasks and funcs
          listeners.append(f'if (auto vec = inst->{method}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->Make{Cast}Vec();')
          listeners.append(f'  inst->{method}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    enterTask_func(obj, nullptr);')
          listeners.append( '    auto* tf = clone(obj, inst);')
          listeners.append( '    if (!tf->VpiName().empty()) {')
          listeners.append( '      ComponentMap& funcMap = std::get<3>(m_instStack.at(m_instStack.size() - 2));')
          listeners.append( '      auto it = funcMap.find(tf->VpiName());')
          listeners.append( '      if (it != funcMap.end()) funcMap.erase(it);')
          listeners.append( '      funcMap.emplace(tf->VpiName(), tf);')
          listeners.append( '    }')
          listeners.append( '    leaveTask_func(obj, nullptr);')
          listeners.append( '    clone_vec->push_back(tf);')
          listeners.append( '  }')
          listeners.append( '}')

        elif method in ['Cont_assigns', 'Primitives', 'Primitive_arrays', 'Ports']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto vec = inst->{method}()) inst->{method}(clone(vec, inst));')

        elif method in ['Gen_scope_arrays']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto vec = inst->{method}()) inst->{method}(clone(vec, inst));')
          # We also want to clone the module cont assign
          listeners.append(f'if (auto vec = defMod->{method}()) {{')
          listeners.append(f'  if (inst->{method}() == nullptr) {{')
          listeners.append(f'    auto clone_vec = m_serializer->Make{Cast}Vec();')
          listeners.append(f'    inst->{method}(clone_vec);')
          listeners.append( '  }')
          listeners.append(f'  auto clone_vec = inst->{method}();')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    clone_vec->push_back(clone(obj, inst));')
          listeners.append( '  }')
          listeners.append( '}')

        elif method in ['Typespecs']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto vec = defMod->{method}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->Make{Cast}Vec();')
          listeners.append(f'  inst->{method}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    if (uniquifyTypespec()) {')
          listeners.append( '      clone_vec->push_back(clone(obj, inst));')
          listeners.append( '    } else {')
          listeners.append( '      clone_vec->push_back(obj);')
          listeners.append( '    }')
          listeners.append( '  }')
          listeners.append( '}')

        elif method not in ['Nets', 'Parameters', 'Param_assigns', 'Interface_arrays', 'Module_arrays']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto vec = defMod->{method}()) inst->{method}(clone(vec, inst));')

    classname = models[classname]['extends']

  return listeners


def _generate_class_listeners(models):
  listeners = []

  for model in models.values():
    modeltype = model.get('type')
    if modeltype != 'obj_def':
      continue

    classname = model.get('name')
    if classname != 'class_defn':
      continue

    while classname:
      model = models[classname]

      for key, value in model.allitems():
        if key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
          name = value.get('name')
          type = value.get('type')
          card = value.get('card')

          cast = 'any' if key == 'group_ref' else type
          Cast = cast[:1].upper() + cast[1:]
          method = name[:1].upper() + name[1:]

          if (card == 'any') and not method.endswith('s'):
            method += 's'

          if card == '1':
            listeners.append(f'if (auto obj = cl->{method}()) cl->{method}(clone(obj, cl));')

          elif method == 'Deriveds':
            # Don't deep clone
            listeners.append(f'if (auto vec = cl->{method}()) cl->{method}(cloneT(vec));')

          else:
            listeners.append(f'if (auto vec = cl->{method}()) cl->{method}(clone(vec, cl));')

      classname = models[classname]['extends']

  return listeners


_copy_overrides = set([
  'begin',
  'bit_select',
  'class_defn',
  'class_typespec',
  'clocking_block',
  'design',
  'disable',
  'foreach_stmt',
  'fork_stmt',
  'func_call',
  'function',
  'gen_scope',
  'indexed_part_select',
  'instance',
  'int_typespec',
  'interface_inst',
  'method_func_call',
  'method_task_call',
  'modport',
  'module_inst',
  'named_begin',
  'named_fork',
  'nets',
  'package',
  'part_select',
  'ports',
  'process_stmt',
  'ref_obj',
  'ref_typespec',
  'ref_var',
  'task',
  'task_call',
  'task_func',
  'tchk',
  'typespec',
  'udp',
  'var_select',
  'variables',
])

def _generate_copy_implementations(models):
  declarations = []
  implementations = []

  for model in models.values():
    modeltype = model.get('type')
    if modeltype == 'group_def':
        continue

    classname = model['name']
    Classname = classname[0].upper() + classname[1:]

    if classname not in _copy_overrides:
      continue

    if modeltype != 'group_def':
        declarations.append(f'  void copy(const {classname}* source, {classname}* target) override;')

    implementations.append(f'void Elaborator::copy(const {classname}* source, {classname}* target) {{')
    if modeltype != 'class_def':
        implementations.append(f'  enter{Classname}(target, nullptr);')

    if classname in ['bit_select']:
        implementations.append(f'  ExprEval eval;')
        implementations.append(f'  bool invalidValue = false;')
        implementations.append( '  const any* const parent = source->VpiParent();')
        implementations.append( '  if (any* val = eval.reduceExpr(source->VpiIndex(), invalidValue, parent, parent, true)) {')
        implementations.append( '    if (!invalidValue) {')
        implementations.append(f'      std::string indexName = eval.prettyPrint(val);')
        implementations.append( '      if (any* indexVal = bindAny(indexName)) {')
        implementations.append(f'        val = eval.reduceExpr(indexVal, invalidValue, parent, parent, true);')
        implementations.append(f'        if (!invalidValue) indexName = eval.prettyPrint(val);')
        implementations.append( '      }')
        implementations.append( '      const std::string_view name(source->VpiName());')
        implementations.append( '      std::string fullIndexName(name);')
        implementations.append( '      fullIndexName.append("[").append(indexName).append("]");')
        implementations.append(f'      target->Actual_group(bindAny(fullIndexName));')
        implementations.append(f'      if (!target->Actual_group()) target->Actual_group(bindAny(name));')
        implementations.append(f'      if (!target->Actual_group()) target->Actual_group((any*) source->Actual_group());')
        implementations.append( '    }')
        implementations.append( '  }')

    implementations.append(f'  copy(static_cast<const {classname}::basetype_t*>(source), static_cast<{classname}::basetype_t*>(target));')
    vpi_name = config.make_vpi_name(classname)

    if 'Select' in vpi_name:
        implementations.append('  if (any* n = bindNet(source->VpiName())) {')
        implementations.append('    if (net* nn = any_cast<net*>(n))')
        implementations.append('      target->VpiFullName(nn->VpiFullName());')
        implementations.append('  }')

    for key, value in model.allitems():
        if key not in ['class', 'obj_ref', 'class_ref', 'group_ref', 'property']:
          continue

        name = value.get('name')
        type = value.get('type')
        card = value.get('card')
        vpi = value.get('vpi')

        cast = 'any' if key == 'group_ref' else type
        Cast = cast[:1].upper() + cast[1:]
        method = name[:1].upper() + name[1:]

        if (card == 'any') and not method.endswith('s'):
            method += 's'

        # Unary relations
        if card == '1':
            if key == 'property':
                if type == 'string' and vpi not in ['vpiFullName']:
                    method = vpi[:1].upper() + vpi[1:]
                    implementations.append(f'  target->{method}(source->{method}());')

            elif (classname in ['ref_obj', 'ref_var']) and (method == 'Actual_group'):
                implementations.append(f'  if (!target->{method}()) target->{method}(bindAny(source->VpiName()));')
                implementations.append(f'  if (!target->{method}()) target->{method}((any*) source->{method}());')

            elif (classname in ['ref_typespec']) and (method == 'Actual_typespec'):
                implementations.append( '  if (uniquifyTypespec()) {')
                implementations.append(f'    if (auto obj = source->{method}()) target->{method}(clone(obj, target));')
                implementations.append( '  } else {')
                implementations.append(f'    if (auto obj = source->{method}()) target->{method}((typespec*) obj);')
                implementations.append( '  }')

            elif (classname == 'udp') and (method == 'Udp_defn'):
                implementations.append(f'  if (!target->{method}()) target->{method}((udp_defn*) bindAny(source->VpiDefName()));')
                implementations.append(f'  if (!target->{method}()) target->{method}((udp_defn*) source->{method}());')

            elif method in ['Task', 'Function']:
                prefix = 'nullptr'
                if 'method_' in classname:
                    implementations.append(f'  const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());')
                    implementations.append( '  const class_var* prefix = nullptr;')
                    implementations.append( '  if (ref) prefix = any_cast<const class_var*>(ref->Actual_group());')
                    prefix = 'prefix'
                implementations.append(f'  scheduleTaskFuncBinding(target, {prefix});')

            elif (classname == 'disable') and (method == 'VpiExpr'):
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((expr*) obj);')

            elif (classname == 'ports') and (method == 'High_conn'):
                implementations.append(f'  if (auto obj = source->{method}()) {{')
                implementations.append( '    ignoreLastInstance(true); ')
                implementations.append(f'    target->{method}(clone(obj, target));')
                implementations.append( '    ignoreLastInstance(false);')
                implementations.append( '  }')

            elif (classname == 'int_typespec') and (method == 'Cast_to_expr'):
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((variables*) obj);')

            elif (classname == 'function') and (method == 'Return'):
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((variables*) obj);')

            elif (classname == 'class_typespec') and (method == 'Class_defn'):
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((class_defn*) obj);')

            elif method == 'Instance':
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((instance*) obj);')
                implementations.append( '  if (instance* inst = target->VpiParent<instance>()) target->Instance(inst);')

            elif method == 'Module_inst':
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((module_inst*) obj);')

            elif method == 'Interface_inst':
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}((interface_inst*) obj);')

            else:
                implementations.append(f'  if (auto obj = source->{method}()) target->{method}(clone(obj, target));')

        elif (classname == 'module_inst') and (method == 'Ref_modules'):
            pass # No cloning

        elif (method == 'Typespecs') or ((classname == 'class_defn') and (method == 'Deriveds')):
            # Don't deep clone
            implementations.append(f'  if (auto vec = source->{method}()) target->{method}(cloneT(vec));')

        else:
            # N-ary relations
            implementations.append(f'  if (auto vec = source->{method}()) target->{method}(clone(vec, target));')

    if modeltype != 'class_def':
        implementations.append(f'  leave{Classname}(target, nullptr);')

    implementations.append('}')
    implementations.append('')

  return declarations, implementations[:-1]


def generate(models):
  module_listeners = _generate_module_listeners(models, 'module_inst')
  interface_listeners = _generate_module_listeners(models, 'interface_inst')
  class_listeners = _generate_class_listeners(models)
  copy_declarations, copy_implementations = _generate_copy_implementations(models)

  with open(config.get_template_filepath('Elaborator.h'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<COPY_DECLARATIONS>', '\n'.join(sorted(copy_declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Elaborator.h'), file_content)

  with open(config.get_template_filepath('Elaborator.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<MODULE_ELABORATOR_LISTENER>', (' ' * 6) + ('\n' + (' ' * 6)).join(module_listeners))
  file_content = file_content.replace('//<INTERFACE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(interface_listeners))
  file_content = file_content.replace('//<CLASS_ELABORATOR_LISTENER>', (' ' * 2) + ('\n' + (' ' * 2)).join(class_listeners))
  file_content = file_content.replace('//<COPY_IMPLEMENTATIONS>', '\n'.join(copy_implementations))
  file_utils.set_content_if_changed(config.get_output_source_filepath('Elaborator.cpp'), file_content)

  return True


def _main():
  import loader

  config.configure()

  models = loader.load_models()
  return generate(models)


if __name__ == '__main__':
  import sys
  sys.exit(0 if _main() else 1)
