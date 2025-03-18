import os

import config
import file_utils
import uhdm_types_h


def generate(models):
    save_ids = []
    save_objects = []
    saves_adapters = []

    restore_ids = []
    restore_objects = []
    restore_adapters = []

    type_map = uhdm_types_h.get_type_map(models)

    for model in models.values():
        modeltype = model['type']
        if modeltype == 'group_def':
            continue

        classname = model['name']
        ClassName = config.make_class_name(classname)
        varName = config.make_var_name(classname, '1')

        basename = model.get('extends', 'BaseClass') or 'BaseClass'
        BaseName = config.make_class_name(basename)

        if modeltype != 'class_def':
            save_ids.append(f'  m_{varName}Factory.mapToIndex(idMap);')
            save_objects.append(f'  adapter.template operator()<{ClassName}, ::{ClassName}>(this, idMap, cap_root.initFactory{ClassName}(m_factories[UhdmType::{ClassName}]->m_objects.size()));')

            restore_ids.append(f'  make<{ClassName}>(cap_root.getFactory{ClassName}().size());')
            restore_objects.append(f'  adapter.template operator()<{ClassName}, ::{ClassName}>(cap_root.getFactory{ClassName}(), this);')

        saves_adapters.append(f'  void operator()(const {ClassName} *const obj, Serializer *const serializer, const Serializer::IdMap &idMap, ::{ClassName}::Builder builder) const {{')
        saves_adapters.append(f'    operator()(static_cast<const {BaseName}*>(obj), serializer, idMap, builder.getBase());')

        restore_adapters.append(f'  void operator()(::{ClassName}::Reader reader, Serializer *const serializer, {ClassName} *const obj) const {{')
        restore_adapters.append(f'    operator()(reader.getBase(), serializer, static_cast<{BaseName}*>(obj));')

        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if name == 'type':
                    continue

                FuncName = config.make_func_name(name, card)

                if type in ['string', 'value', 'delay']:
                    saves_adapters.append(f'    builder.set{FuncName}((RawSymbolId)serializer->m_symbolFactory.registerSymbol(obj->get{FuncName}()));')
                    restore_adapters.append(f'    obj->set{FuncName}(serializer->m_symbolFactory.getSymbol(SymbolId(reader.get{FuncName}(), kUnknownRawSymbol)));')
                else:
                    saves_adapters.append(f'    builder.set{FuncName}(obj->get{FuncName}());')
                    restore_adapters.append(f'    obj->set{FuncName}(reader.get{FuncName}());')

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')

                if key == 'group_ref':
                    type = 'any'

                TypeName = config.make_class_name(type)
                FuncName = config.make_func_name(name, card)
                varName = config.make_var_name(type, '1')

                if card == '1':
                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'    if (auto p = obj->get{FuncName}()) {{')
                        saves_adapters.append(f'      ::ObjIndexType::Builder tmp = builder.get{FuncName}();')
                        saves_adapters.append( '      tmp.setIndex(getId(p, idMap));')
                        saves_adapters.append(f'      tmp.setType(static_cast<uint32_t>(p->getUhdmType()));')
                        saves_adapters.append( '    }')

                        restore_adapters.append(f'    obj->set{FuncName}(serializer->getObject<{TypeName}>(reader.get{FuncName}().getType(), reader.get{FuncName}().getIndex() - 1));')
                    else:
                        saves_adapters.append(f'    if (auto p = obj->get{FuncName}()) builder.set{FuncName}(getId(p, idMap));')

                        restore_adapters.append(f'    if (reader.get{FuncName}()) {{')
                        restore_adapters.append(f'      obj->set{FuncName}(serializer->getObject<{TypeName}>(static_cast<uint32_t>(UhdmType::{TypeName}), reader.get{FuncName}() - 1));')
                        restore_adapters.append( '    }')

                else:
                    obj_key = '::ObjIndexType' if key in ['class_ref', 'group_ref'] else '::uint64_t'

                    saves_adapters.append(f'    if (auto v = obj->get{FuncName}()) {{')
                    saves_adapters.append(f'      ::capnp::List<{obj_key}>::Builder {varName}Builder = builder.init{FuncName}(v->size());')
                    saves_adapters.append( '      for (int32_t i = 0, n = v->size(); i < n; ++i) {')

                    restore_adapters.append(f'    if (uint32_t n = reader.get{FuncName}().size()) {{')
                    restore_adapters.append(f'      std::vector<{TypeName}*> *const v = serializer->makeCollection<{TypeName}>();')
                    restore_adapters.append( '      v->reserve(n);')
                    restore_adapters.append( '      for (uint32_t i = 0; i < n; ++i) {')

                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'        ::ObjIndexType::Builder tmp = {varName}Builder[i];')
                        saves_adapters.append( '        tmp.setIndex(getId((*v)[i], idMap));')
                        saves_adapters.append( '        tmp.setType(static_cast<uint32_t>(((*v)[i])->getUhdmType()));')

                        restore_adapters.append(f'        v->emplace_back(serializer->getObject<{TypeName}>(reader.get{FuncName}()[i].getType(), reader.get{FuncName}()[i].getIndex() - 1));')
                    else:
                        saves_adapters.append(f'        {varName}Builder.set(i, getId((*v)[i], idMap));')

                        restore_adapters.append(f'        v->emplace_back(serializer->getObject<{TypeName}>(static_cast<uint32_t>(UhdmType::{TypeName}), reader.get{FuncName}()[i] - 1));')

                    saves_adapters.append('      }')
                    saves_adapters.append('    }')

                    restore_adapters.append( '      }')
                    restore_adapters.append(f'      obj->set{FuncName}(v);')
                    restore_adapters.append( '    }')

        saves_adapters.append('  }')
        saves_adapters.append('')

        restore_adapters.append('  }')
        restore_adapters.append('')

    uhdm_name_map = [ f'    case UhdmType::{name} /* = {id} */: return "{name}";' for name, id in type_map.items() if name != 'BaseClass' ]
    init_factories = [ f'  m_factories[UhdmType::{name}] = new Factory;' for name in type_map.keys() ]

    # Serializer.h
    with open(config.get_template_filepath('Serializer.h'), 'rt') as strm:
        file_content = strm.read()

    file_utils.set_content_if_changed(config.get_output_header_filepath('Serializer.h'), file_content)

    # Serializer.cpp
    with open(config.get_template_filepath('Serializer.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_ID>', '\n'.join(sorted(save_ids)))
    file_content = file_content.replace('<INIT_FACTORIES>', '\n'.join(init_factories))
    file_content = file_content.replace('<UHDM_NAME_MAP>', '\n'.join(uhdm_name_map))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer.cpp'), file_content)

    # Serializer_save.cpp
    with open(config.get_template_filepath('Serializer_save.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_SAVE>', '\n'.join(sorted(save_objects)))
    file_content = file_content.replace('<CAPNP_SAVE_ADAPTERS>', '\n'.join(saves_adapters))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer_save.cpp'), file_content)

    # Serializer_restore.cpp
    with open(config.get_template_filepath('Serializer_restore.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_INIT_FACTORIES>', '\n'.join(sorted(restore_ids)))
    file_content = file_content.replace('<CAPNP_RESTORE_FACTORIES>', '\n'.join(sorted(restore_objects)))
    file_content = file_content.replace('<CAPNP_RESTORE_ADAPTERS>', '\n'.join(restore_adapters))
    file_utils.set_content_if_changed(config.get_output_source_filepath('Serializer_restore.cpp'), file_content)

    return True


def _main():
    import loader

    config.configure()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
