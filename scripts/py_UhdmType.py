from collections import OrderedDict

import config
import file_utils
import uhdm_types_h


def generate(models):
    types = '\n'.join([f'      .value("{name}", uhdm::UhdmType::{name})' for name in uhdm_types_h.get_type_map(models).keys()])

    with open(config.get_template_filepath('py_UhdmType.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('// <ENUM_VALUES>', types)
    file_utils.set_content_if_changed(config.get_output_python_source_filepath('py_UhdmType.cpp'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
