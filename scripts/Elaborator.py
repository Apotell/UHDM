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
          listeners.append(f'if (auto vec = inst->{method}()) inst->{method}(DeepClone(vec, inst));')

        elif method in ['Gen_scope_arrays']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto vec = inst->{method}()) inst->{method}(DeepClone(vec, inst));')
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
          listeners.append(f'if (auto vec = defMod->{method}()) inst->{method}(DeepClone(vec, inst));')

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
            listeners.append(f'if (auto vec = cl->{method}()) cl->{method}(Clone(vec));')

          else:
            listeners.append(f'if (auto vec = cl->{method}()) cl->{method}(DeepClone(vec, cl));')

      classname = models[classname]['extends']

  return listeners


def _generate_clone_case_statements(models):
  case_statements = []

  for model in models.values():
    type = model['type']
    if type == 'obj_def':
        classname = model['name']
        case_statements.append(f'    case UHDM_OBJECT_TYPE::uhdm{classname}: target = DeepClone(static_cast<const {classname}*>(source), parent); break;')

  return case_statements


def _generate_copy_declarations(models):
  declarations = []

  for model in models.values():
    type = model['type']

    if type != 'group_def':
      classname = model['name']
      declarations.append(f'  void DeepCopy(const {classname}* source, {classname}* target);')

  return declarations


def _generate_copy_implementations(models):
  any_implementations = []

  for model in models.values():
    modeltype = model.get('type')
    if modeltype == 'group_def':
        continue

    classname = model['name']
    # if '_call' in classname or classname in [ 'function', 'task', 'constant', 'tagged_pattern', 'gen_scope_array', 'hier_path', 'cont_assign' ]:
    #     continue

    Classname = classname[0].upper() + classname[1:]
    any_implementations.append(f'void Elaborator::DeepCopy(const {classname}* source, {classname}* target) {{')

    if modeltype != 'class_def':
        any_implementations.append(f'  enter{Classname}(target, nullptr);')

    if classname in ['bit_select']:
        any_implementations.append(f'  ExprEval eval;')
        any_implementations.append(f'  bool invalidValue = false;')
        any_implementations.append( '  const any* const parent = source->VpiParent();')
        any_implementations.append( '  if (any* val = eval.reduceExpr(source->VpiIndex(), invalidValue, parent, parent, true)) {')
        any_implementations.append( '    if (!invalidValue) {')
        any_implementations.append(f'      std::string indexName = eval.prettyPrint(val);')
        any_implementations.append( '      if (any* indexVal = bindAny(indexName)) {')
        any_implementations.append(f'        val = eval.reduceExpr(indexVal, invalidValue, parent, parent, true);')
        any_implementations.append(f'        if (!invalidValue) indexName = eval.prettyPrint(val);')
        any_implementations.append( '      }')
        any_implementations.append( '      const std::string_view name(source->VpiName());')
        any_implementations.append( '      std::string fullIndexName(name);')
        any_implementations.append( '      fullIndexName.append("[").append(indexName).append("]");')
        any_implementations.append(f'      target->Actual_group(bindAny(fullIndexName));')
        any_implementations.append(f'      if (!target->Actual_group()) target->Actual_group(bindAny(name));')
        any_implementations.append(f'      if (!target->Actual_group()) target->Actual_group((any*) source->Actual_group());')
        any_implementations.append( '    }')
        any_implementations.append( '  }')

    any_implementations.append(f'  DeepCopy(static_cast<const {classname}::basetype_t*>(source), static_cast<{classname}::basetype_t*>(target));')
    vpi_name = config.make_vpi_name(classname)

    if 'Select' in vpi_name:
        any_implementations.append('  if (any* n = bindNet(source->VpiName())) {')
        any_implementations.append('    if (net* nn = any_cast<net*>(n))')
        any_implementations.append('      target->VpiFullName(nn->VpiFullName());')
        any_implementations.append('  }')

    for key, value in model.allitems():
        if key not in ['class', 'obj_ref', 'class_ref', 'group_ref']:
          continue

        name = value.get('name')
        type = value.get('type')
        card = value.get('card')

        cast = 'any' if key == 'group_ref' else type
        Cast = cast[:1].upper() + cast[1:]
        method = name[:1].upper() + name[1:]

        if (card == 'any') and not method.endswith('s'):
            method += 's'

        # Unary relations
        if card == '1':
            if (classname in ['ref_obj', 'ref_var']) and (method == 'Actual_group'):
                any_implementations.append(f'  if (!target->{method}()) target->{method}(bindAny(source->VpiName()));')
                any_implementations.append(f'  if (!target->{method}()) target->{method}((any*) source->{method}());')

            elif (classname in ['ref_typespec']) and (method == 'Actual_typespec'):
                any_implementations.append( '  if (uniquifyTypespec()) {')
                any_implementations.append(f'    if (auto obj = source->{method}()) target->{method}(clone(obj, target));')
                any_implementations.append( '  } else {')
                any_implementations.append(f'    if (auto obj = source->{method}()) target->{method}((typespec*) obj);')
                any_implementations.append( '  }')

            elif (classname == 'udp') and (method == 'Udp_defn'):
                any_implementations.append(f'  if (!target->{method}()) target->{method}((udp_defn*) bindAny(source->VpiDefName()));')
                any_implementations.append(f'  if (!target->{method}()) target->{method}((udp_defn*) source->{method}());')

            elif method in ['Task', 'Function']:
                prefix = 'nullptr'
                if 'method_' in classname:
                    any_implementations.append(f'  const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());')
                    any_implementations.append( '  const class_var* prefix = nullptr;')
                    any_implementations.append( '  if (ref) prefix = any_cast<const class_var*>(ref->Actual_group());')
                    prefix = 'prefix'
                any_implementations.append(f'  scheduleTaskFuncBinding(target, {prefix});')

            elif (classname == 'disable') and (method == 'VpiExpr'):
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((expr*) obj);')

            elif (classname == 'ports') and (method == 'High_conn'):
                any_implementations.append(f'  if (auto obj = source->{method}()) {{')
                any_implementations.append( '    ignoreLastInstance(true); ')
                any_implementations.append(f'    target->{method}(clone(obj, target));')
                any_implementations.append( '    ignoreLastInstance(false);')
                any_implementations.append( '  }')

            elif (classname == 'int_typespec') and (method == 'Cast_to_expr'):
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((variables*) obj);')

            elif (classname == 'function') and (method == 'Return'):
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((variables*) obj);')

            elif (classname == 'class_typespec') and (method == 'Class_defn'):
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((class_defn*) obj);')

            elif method == 'Instance':
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((instance*) obj);')
                any_implementations.append( '  if (instance* inst = target->VpiParent<instance>()) target->Instance(inst);')

            elif method == 'Module_inst':
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((module_inst*) obj);')

            elif method == 'Interface_inst':
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}((interface_inst*) obj);')

            else:
                any_implementations.append(f'  if (auto obj = source->{method}()) target->{method}(clone(obj, target));')

        elif (classname == 'module_inst') and (method == 'Ref_modules'):
            pass # No cloning

        elif (method == 'Typespecs') or ((classname == 'class_defn') and (method == 'Deriveds')):
            # Don't deep clone
            any_implementations.append(f'  if (auto vec = source->{method}()) target->{method}(Clone(vec));')

        else:
            # N-ary relations
            any_implementations.append(f'  if (auto vec = source->{method}()) target->{method}(DeepClone(vec, target));')

    if modeltype != 'class_def':
        any_implementations.append(f'  leave{Classname}(target, nullptr);')

    any_implementations.append('}')
    any_implementations.append('')

  return any_implementations

def generate(models):
  module_listeners = _generate_module_listeners(models, 'module_inst')
  interface_listeners = _generate_module_listeners(models, 'interface_inst')
  class_listeners = _generate_class_listeners(models)

  copy_any_declarations = _generate_copy_declarations(models)
  copy_any_implementations = _generate_copy_implementations(models)

  clone_case_statements = _generate_clone_case_statements(models)

  with open(config.get_template_filepath('Elaborator.h'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<COPY_ANY_DECLARATIONS>', '\n'.join(sorted(copy_any_declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Elaborator.h'), file_content)

  with open(config.get_template_filepath('Elaborator.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<MODULE_ELABORATOR_LISTENER>', (' ' * 6) + ('\n' + (' ' * 6)).join(module_listeners))
  file_content = file_content.replace('//<INTERFACE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(interface_listeners))
  file_content = file_content.replace('//<CLASS_ELABORATOR_LISTENER>', (' ' * 2) + ('\n' + (' ' * 2)).join(class_listeners))
  file_content = file_content.replace('//<CLONE_CASE_STATEMENTS>', '\n'.join(sorted(clone_case_statements)))
  file_content = file_content.replace('//<COPY_ANY_IMPLEMENTATIONS>', '\n'.join(copy_any_implementations))
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
