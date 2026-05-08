import config
import file_utils
from collections import OrderedDict


def generate(models):
    types = set([name for name, model in models.items() if model['type'] != 'group_def'])
    types.add('any')
    types.discard('BaseClass')

    containers = []
    for type in sorted(types):
        containers.append(f'  typedef std::vector<{type}*> VectorOf{type};')
        containers.append(f'  typedef std::vector<{type}*>::iterator VectorOf{type}Itr;')
        containers.append('')
    containers = '\n'.join(containers)

    with open(config.get_template_filepath('containers.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CONTAINERS>', containers)
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
