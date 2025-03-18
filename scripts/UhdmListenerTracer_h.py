import config
import file_utils
import uhdm_types_h


def generate(models):
    object_methods = []
    for model in models.values():
        if model['type'] not in ['class_def', 'group_def']:
            classname = model['name']
            ClassName = config.make_class_name(classname)

            object_methods.append(f'    void enter{ClassName}(const {ClassName}* const object, uint32_t vpiRelation = 0) final {{ TRACE_ENTER; }}')
            object_methods.append(f'    void leave{ClassName}(const {ClassName}* const object, uint32_t vpiRelation = 0) final {{ TRACE_LEAVE; }}')
            object_methods.append('')

    collection_methods = []
    for TypeName in sorted(uhdm_types_h.get_type_map(models).keys()):
        if TypeName != 'BaseClass':
            collection_methods.append(f'    void enter{TypeName}Collection(const Any* const object, const {TypeName}Collection& objects, uint32_t vpiRelation = 0) final {{ TRACE_ENTER; }}')
            collection_methods.append(f'    void leave{TypeName}Collection(const Any* const object, const {TypeName}Collection& objects, uint32_t vpiRelation = 0) final {{ TRACE_LEAVE; }}')
            collection_methods.append('')

    with open(config.get_template_filepath('UhdmListenerTracer.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<UHDM_LISTENER_OBJECT_TRACER_METHODS>', '\n'.join(object_methods))
    file_content = file_content.replace('<UHDM_LISTENER_COLLECTION_TRACER_METHODS>', '\n'.join(collection_methods))
    file_utils.set_content_if_changed(config.get_output_header_filepath('UhdmListenerTracer.h'), file_content)
    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
