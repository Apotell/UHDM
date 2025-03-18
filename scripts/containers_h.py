import config
import file_utils
import uhdm_types_h


def generate(models):
    types = list(uhdm_types_h.get_type_map(models).keys())
    
    containers = [
      '  using SymbolCollection = std::vector<SymbolId>;',
    ] + [
      f'  using {config.make_class_name(type)}Collection = std::vector<{config.make_class_name(type)}*>;' for type in types
    ]

    with open(config.get_template_filepath('containers.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CONTAINERS>', '\n'.join(sorted(containers)))
    file_utils.set_content_if_changed(config.get_output_header_filepath('containers.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
