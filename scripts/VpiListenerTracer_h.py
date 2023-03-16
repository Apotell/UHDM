import config
import file_utils
import uhdm_types_h

def generate(models):
    methods = []
    for model in models.values():
        if model['type'] not in ['class_def', 'group_def']:
            classname = model['name']
            ClassName = config.make_class_name(classname)

            methods.append(f'    void enter{ClassName}(const {ClassName}* object, vpiHandle handle) final {{ TRACE_ENTER; }}')
            methods.append(f'    void leave{ClassName}(const {ClassName}* object, vpiHandle handle) final {{ TRACE_LEAVE; }}')
            methods.append('')

    with open(config.get_template_filepath('VpiListenerTracer.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENER_TRACER_METHODS>', '\n'.join(methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('VpiListenerTracer.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
