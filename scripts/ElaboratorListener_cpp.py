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
        vpi = value.get('vpi')

        TypeName = config.make_class_name('any' if key == 'group_ref' else type)
        FuncName = config.make_func_name(name, card)

        if card == '1':
          suffix = 'Obj 'if vpi in ['vpiName'] else ''
          listeners.append(f'if (auto obj = defMod->get{FuncName}{suffix}()) {{')
          listeners.append(f'  inst->set{FuncName}{suffix}(obj->deepClone(inst, m_context));')
          listeners.append( '}')

        elif FuncName in ['RefModules', 'GenStmts']:
          pass # No elab

        elif FuncName in ['TaskFuncs']:
          # We want to deep clone existing instance tasks and funcs
          listeners.append(f'if (auto vec = inst->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 1')
          listeners.append(f'  inst->set{FuncName}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    enterTaskFunc(obj, nullptr);')
          listeners.append( '    auto* tf = obj->deepClone(inst, m_context);')
          listeners.append( '    if (!tf->getName().empty()) {')
          listeners.append( '      ComponentMap& funcMap = std::get<3>(m_instStack.at(m_instStack.size() - 2));')
          listeners.append( '      auto it = funcMap.find(tf->getName());')
          listeners.append( '      if (it != funcMap.end()) funcMap.erase(it);')
          listeners.append( '      funcMap.emplace(tf->getName(), tf);')
          listeners.append( '    }')
          listeners.append( '    leaveTaskFunc(obj, nullptr);')
          listeners.append( '    tf->setParent(inst);')
          listeners.append( '    clone_vec->emplace_back(tf);')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName in ['ContAssigns', 'Primitives', 'PrimitiveArrays', 'Ports']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto vec = inst->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 2')
          listeners.append(f'  inst->set{FuncName}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    clone_vec->emplace_back(obj->deepClone(inst, m_context));')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName in ['GenScopeArrays']:
          # We want to deep clone existing instance cont assign to perform binding
          listeners.append(f'if (auto vec = inst->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 3')
          listeners.append(f'  inst->set{FuncName}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    clone_vec->emplace_back(obj->deepClone(inst, m_context));')
          listeners.append( '  }')
          listeners.append( '}')
          # We also want to clone the module cont assign
          listeners.append(f'if (auto vec = defMod->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = inst->get{FuncName}(true);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    clone_vec->emplace_back(obj->deepClone(inst, m_context));')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName in ['Typespecs']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto vec = defMod->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 5')
          listeners.append(f'  inst->set{FuncName}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    if (uniquifyTypespec()) {')
          listeners.append( '      clone_vec->emplace_back(obj->deepClone(inst, m_context));')
          listeners.append( '    } else {')
          listeners.append( '      clone_vec->emplace_back(obj);')
          listeners.append( '    }')
          listeners.append( '  }')
          listeners.append( '}')

        elif FuncName not in ['Nets', 'Parameters', 'ParamAssigns', 'InterfaceArrays', 'ModuleArrays']:
          # We don't want to override the elaborated instance ports by the module def ports, same for nets, params and param_assigns
          listeners.append(f'if (auto vec = defMod->get{FuncName}()) {{')
          listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 6')
          listeners.append(f'  inst->set{FuncName}(clone_vec);')
          listeners.append( '  for (auto obj : *vec) {')
          listeners.append( '    clone_vec->emplace_back(obj->deepClone(inst, m_context));')
          listeners.append( '  }')
          listeners.append( '}')

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
          vpi = value.get('vpi')

          TypeName = config.make_class_name('any' if key == 'group_ref' else type)
          FuncName = config.make_func_name(name, card)
          varName = config.make_var_name(name, card)

          if card == '1':
            suffix = 'Obj 'if vpi in ['vpiName'] else ''
            listeners.append(f'if (auto obj = cl->get{FuncName}{suffix}()) {{')
            listeners.append(f'  cl->set{FuncName}{suffix}(obj->deepClone(cl, m_context));')
            listeners.append( '}')

          elif FuncName == 'DerivedClasses':
            # Don't deep clone
            listeners.append(f'if (auto {varName} = cl->get{FuncName}()) {{')
            listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 7')
            listeners.append(f'  cl->set{FuncName}(clone_vec);')
            listeners.append(f'  clone_vec->insert(clone_vec->cend(), {varName}->cbegin(), {varName}->cend());')
            listeners.append( '}')

          else:
            listeners.append(f'if (auto {varName} = cl->get{FuncName}()) {{')
            listeners.append(f'  auto clone_vec = m_serializer->makeCollection<{TypeName}>(); // 8')
            listeners.append(f'  cl->set{FuncName}(clone_vec);')
            listeners.append(f'  for (auto obj : *{varName}) {{')
            listeners.append( '    clone_vec->emplace_back(obj->deepClone(cl, m_context));')
            listeners.append( '  }')
            listeners.append( '}')

      classname = models[classname]['extends']

  return listeners


def generate(models):
  module_listeners = _generate_module_listeners(models, 'module')
  interface_listeners = _generate_module_listeners(models, 'interface')
  class_listeners = _generate_class_listeners(models)

  with open(config.get_template_filepath('ElaboratorListener.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('<MODULE_ELABORATOR_LISTENER>', (' ' * 6) + ('\n' + (' ' * 6)).join(module_listeners))
  file_content = file_content.replace('<INTERFACE_ELABORATOR_LISTENER>', (' ' * 10) + ('\n' + (' ' * 10)).join(interface_listeners))
  file_content = file_content.replace('<CLASS_ELABORATOR_LISTENER>', (' ' * 2) + ('\n' + (' ' * 2)).join(class_listeners))
  file_utils.set_content_if_changed(config.get_output_source_filepath('ElaboratorListener.cpp'), file_content)

  return True


def _main():
  import loader

  config.configure()

  models = loader.load_models()
  return generate(models)


if __name__ == '__main__':
  import sys
  sys.exit(0 if _main() else 1)
