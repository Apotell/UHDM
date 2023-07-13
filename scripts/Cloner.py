import config
import file_utils


def generate(models):
  classnames = set([
    model['name'] for model in models.values() if model['type'] == 'obj_def'
  ])

  declarations = []
  implementations = []
  any_case_statements = []

  for classname in sorted(classnames):
    Classname_ = classname[:1].upper() + classname[1:]

    declarations.append(f'  virtual any* clone{Classname_}(const {classname}* source, any* parent);')

    implementations.append(f'any* Cloner::clone{Classname_}(const {classname}* source, any* parent) {{')
    implementations.append( '  return source->DeepClone(parent, this);')
    implementations.append(f'}}')
    implementations.append( '')

    any_case_statements.append(f'    case uhdm{classname}: clone = clone{Classname_}(static_cast<const {classname} *>(source), parent); break;')

  # Cloner.h
  with open(config.get_template_filepath('Cloner.h'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<CLONER_DECLARATIONS>', '\n'.join(sorted(declarations)))
  file_utils.set_content_if_changed(config.get_output_header_filepath('Cloner.h'), file_content)

  # Cloner.cpp
  with open(config.get_template_filepath('Cloner.cpp'), 'rt') as strm:
      file_content = strm.read()

  file_content = file_content.replace('//<CLONER_IMPLEMENTATIONS>', '\n'.join(implementations))
  file_content = file_content.replace('//<CLONER_CLONEANY_CASE_STATEMENTS>', '\n'.join(sorted(any_case_statements)))
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
