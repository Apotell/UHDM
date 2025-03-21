from collections import OrderedDict

import config
import file_utils


def _get_implementation(name, type, vpi, card):
  varName = config.make_var_name(name, card)
  FuncName = config.make_func_name(name, card)

  content = []
  if card == '1':
    if vpi not in ['vpiName', 'vpiType', 'vpiFile', 'vpiLineNo', 'vpiColumnNo', 'vpiEndLineNo', 'vpiEndColumnNo']:
      if type in ['string', 'value', 'delay']:
        content.append(f'  if (std::string_view s = object->get{FuncName}(); isValid(s)) {{')
        content.append(f'    appendIndent(indent + kLevelIndent) << "{varName}: " << s << "\\n";') # no std::endl, avoid flush
        content.append( '  }')

      elif type in ['int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t', 'bool']:
        content.append(f'  appendIndent(indent + kLevelIndent) << "{varName}: " << object->get{FuncName}() << "\\n";') # no std::endl, avoid flush

      else:
        content.append(f'  if (const Any *const p = object->get{FuncName}()) {{')
        content.append(f'    appendIndent(indent + kLevelIndent) << "{varName}: ";')
        content.append( '    appendObject(p, ", ") << "\\n";') # no std::endl, avoid flush
        content.append( '  }')
  else:
    TypeName = config.make_class_name(type)

    content.append(f'  if (const {TypeName}Collection *const c = object->get{FuncName}()) {{')
    content.append(f'    appendIndent(indent + kLevelIndent) << "{varName}: " << "\\n";')
    content.append(f'    for (const {TypeName} *p : *c) {{')
    content.append(f'      appendIndent(indent + kLevelIndent + kLevelIndent);')
    content.append(f'      appendObject(p, ", ") << "\\n";')
    content.append( '    }')
    content.append( '  }')

  return content


def generate(models):
  case_statements = []
  private_declarations = []
  private_implementations = []

  for model in models.values():
    modeltype = model['type']
    if modeltype == 'group_def':
      continue

    classname = model['name']
    baseclass = model.get('extends', None) or 'BaseClass'

    ClassName = config.make_class_name(classname)
    BaseName = config.make_class_name(baseclass)

    if modeltype == 'obj_def':
      case_statements.append(f'    case UhdmType::{ClassName}: visit{ClassName}(static_cast<const {ClassName}*>(object), indent); break;')

    private_declarations.append(f'  void visit{ClassName}(const {ClassName} *const object, uint32_t indent) const;')
    private_implementations.append(f'void UhdmVisitor::visit{ClassName}(const {ClassName} *const object, uint32_t indent) const {{')
    private_implementations.append(f'  visit{BaseName}(object, indent);')

    for key, value in model.allitems():
      if key in ['property', 'obj_ref', 'class_ref', 'group_ref']:
        name = value.get('name')
        type = value.get('type')
        vpi = value.get('vpi')
        card = value.get('card')

        if key == 'group_ref':
          type = 'any'

        private_implementations.extend(_get_implementation(name, type, vpi, card))

    private_implementations.append(f'}}')
    private_implementations.append('')

  # UhdmVisitor.h
  with open(config.get_template_filepath('UhdmVisitor.h'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<VISITOR_PRIVATE_DECLARATIONS>', '\n'.join(private_declarations))
  file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmVisitor.h'), file_content)

  # UhdmVisitor.cpp
  with open(config.get_template_filepath('UhdmVisitor.cpp'), 'rt') as strm:
    file_content = strm.read()

  file_content = file_content.replace('//<VISITOR_CASE_STATEMENTS>', '\n'.join(sorted(case_statements)))
  file_content = file_content.replace('//<VISITOR_PRIVATE_IMPLEMENTATIONS>', '\n'.join(private_implementations))
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
