import config
import file_utils


def generate(models):
    class_declarations = set(['BaseClass'])
    for model in models.values():
        model_type = model['type']

        if model_type != 'group_def':
            class_declarations.add(config.make_class_name(model['name']))

    class_declarations = [f'  class {name};' for name in class_declarations]

    with open(config.get_template_filepath('uhdm_forward_decl.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_CLASSES_FORWARD_DECL>', '\n'.join(sorted(class_declarations)))
    file_utils.set_content_if_changed(config.get_output_header_filepath('uhdm_forward_decl.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
