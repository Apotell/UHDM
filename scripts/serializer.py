import os

import config
import file_utils
import uhdm_types_h


def generate(models):
    factory_data_members = []
    factory_function_declarations = []
    factory_function_implementations = []
    factory_purge = []
    factory_gc = []
    factory_stats = []
    factory_get_object = []
    factory_erase_object = []
    
    object_accessor_declarations = []
    object_accessor_implementations = []

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
        basename = model.get('extends', 'BaseClass') or 'BaseClass'

        Classname_ = classname[:1].upper() + classname[1:]
        Classname = Classname_.replace('_', '')
        Basename = basename[:1].upper() + basename[1:].replace('_', '')

        if modeltype != 'class_def':
            factory_data_members.append(f'  {classname}Factory {classname}Maker;')
            factory_function_declarations.append(f'  {classname}* Make{Classname_}();')
            factory_function_implementations.append(f'{classname}* Serializer::Make{Classname_}() {{ return Make<{classname}>(&{classname}Maker); }}')
            factory_get_object.append(f'    case UHDM_OBJECT_TYPE::uhdm{classname} /* = {type_map["uhdm" + classname]} */: return {classname}Maker.m_objects[index];')
            factory_erase_object.append(f'    case UHDM_OBJECT_TYPE::uhdm{classname} /* = {type_map["uhdm" + classname]} */: return {classname}Maker.Erase(static_cast<const {classname}*>(p));')

            object_accessor_declarations.append(f'  const std::deque<{classname}*>& get{Classname_}Objects() const;')
            object_accessor_implementations.append(f'const std::deque<{classname}*>& Serializer::get{Classname_}Objects() const {{ return {classname}Maker.m_objects; }}')

            save_ids.append(f'  {classname}Maker.MapToIndex(idMap);')
            save_objects.append(f'  adapter.template operator()<{classname}, {Classname}>({classname}Maker, this, idMap, cap_root.initFactory{Classname}({classname}Maker.m_objects.size()));')

            restore_ids.append(f'  Make(&{classname}Maker, cap_root.getFactory{Classname}().size());')
            restore_objects.append(f'  adapter.template operator()<{classname}, {Classname}>(cap_root.getFactory{Classname}(), this, {classname}Maker.m_objects);')

            factory_purge.append(f'  {classname}Maker.Purge();')
            if classname != 'package':
                factory_gc.append(f'  {classname}Maker.EraseIfNotIn(visited);')
            factory_stats.append(f'  stats.insert(std::make_pair("{classname}", {classname}Maker.m_objects.size()));')

        factory_data_members.append(f'  VectorOf{classname}Factory {classname}VectMaker;')
        factory_function_declarations.append(f'  std::vector<{classname}*>* Make{Classname_}Vec();')
        factory_function_implementations.append(f'std::vector<{classname}*>* Serializer::Make{Classname_}Vec() {{ return Make<{classname}>(&{classname}VectMaker); }}')

        saves_adapters.append(f'  void operator()(const {classname} *const obj, Serializer *const serializer, const Serializer::IdMap &idMap, {Classname}::Builder builder) const {{')
        saves_adapters.append(f'    operator()(static_cast<const {basename}*>(obj), serializer, idMap, builder.getBase());')

        restore_adapters.append(f'  void operator()({Classname}::Reader reader, Serializer *const serializer, {classname} *const obj) const {{')
        restore_adapters.append(f'    operator()(reader.getBase(), serializer, static_cast<{basename}*>(obj));')

        for key, value in model.allitems():
            if key == 'property':
                name = value.get('name')
                vpi = value.get('vpi')
                type = value.get('type')
                card = value.get('card')

                if name == 'type':
                    continue

                Vpi_ = vpi[:1].upper() + vpi[1:]
                Vpi = Vpi_.replace('_', '')

                if type in ['string', 'value', 'delay']:
                    saves_adapters.append(f'    builder.set{Vpi}((RawSymbolId)serializer->symbolMaker.Make(obj->{Vpi_}()));')
                    restore_adapters.append(f'    obj->{Vpi_}(serializer->symbolMaker.GetSymbol(SymbolId(reader.get{Vpi}(), kUnknownRawSymbol)));')
                else:
                    saves_adapters.append(f'    builder.set{Vpi}(obj->{Vpi_}());')
                    restore_adapters.append(f'    obj->{Vpi_}(reader.get{Vpi}());')

            elif key in ['class', 'obj_ref', 'class_ref', 'group_ref']:
                name = value.get('name')
                type = value.get('type')
                card = value.get('card')
                id = value.get('id')

                Type_ = type[:1].upper() + type[1:]
                Type = Type_.replace('_', '')

                if (card == 'any') and not name.endswith('s'):
                    name += 's'

                Name_ = name[:1].upper() + name[1:]
                Name = Name_.replace('_', '')

                real_type = type
                if key == 'group_ref':
                    type = 'any'

                if card == '1':
                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'    if (obj->{Name_}() != nullptr) {{')
                        saves_adapters.append(f'      ::ObjIndexType::Builder tmp = builder.get{Name}();')
                        saves_adapters.append(f'      tmp.setIndex(GetId(obj->{Name_}(), idMap));')
                        saves_adapters.append(f'      tmp.setType(static_cast<uint32_t>((obj->{Name_}())->UhdmType()));')
                        saves_adapters.append( '    }')

                        restore_adapters.append(f'    obj->{Name_}(({type}*)serializer->GetObject(reader.get{Name}().getType(), reader.get{Name}().getIndex() - 1));')
                    else:
                        saves_adapters.append(f'    if (obj->{Name_}() != nullptr) builder.set{Name}(GetId(obj->{Name_}(), idMap));')

                        restore_adapters.append(f'    if (reader.get{Name}()) {{')
                        restore_adapters.append(f'      obj->{Name_}(serializer->{type}Maker.m_objects[reader.get{Name}() - 1]);')
                        restore_adapters.append( '    }')

                else:
                    obj_key = '::ObjIndexType' if key in ['class_ref', 'group_ref'] else '::uint64_t'

                    saves_adapters.append(f'    if (obj->{Name_}() != nullptr) {{')
                    saves_adapters.append(f'      ::capnp::List<{obj_key}>::Builder {Name}s = builder.init{Name}(obj->{Name_}()->size());')
                    saves_adapters.append(f'      for (int32_t i = 0, n = obj->{Name_}()->size(); i < n; ++i) {{')

                    restore_adapters.append(f'    if (uint32_t n = reader.get{Name}().size()) {{')
                    restore_adapters.append(f'      std::vector<{type}*>* vect = serializer->{type}VectMaker.Make();')
                    restore_adapters.append(f'      vect->reserve(n);')
                    restore_adapters.append(f'      for (uint32_t i = 0; i < n; ++i) {{')

                    if key in ['class_ref', 'group_ref']:
                        saves_adapters.append(f'        ::ObjIndexType::Builder tmp = {Name}s[i];')
                        saves_adapters.append(f'        tmp.setIndex(GetId((*obj->{Name_}())[i], idMap));')
                        saves_adapters.append(f'        tmp.setType(static_cast<uint32_t>(((BaseClass*)((*obj->{Name_}())[i]))->UhdmType()));')

                        restore_adapters.append(f'        vect->emplace_back(({type}*)serializer->GetObject(reader.get{Name}()[i].getType(), reader.get{Name}()[i].getIndex() - 1));')
                    else:
                        saves_adapters.append(f'        {Name}s.set(i, GetId((*obj->{Name_}())[i], idMap));')

                        restore_adapters.append(f'        vect->emplace_back(serializer->{type}Maker.m_objects[reader.get{Name}()[i] - 1]);')

                    saves_adapters.append('      }')
                    saves_adapters.append('    }')

                    restore_adapters.append( '      }')
                    restore_adapters.append(f'      obj->{Name_}(vect);')
                    restore_adapters.append( '    }')

        saves_adapters.append('  }')
        saves_adapters.append('')

        restore_adapters.append('  }')
        restore_adapters.append('')

    uhdm_name_map = [ f'    case UHDM_OBJECT_TYPE::{name} /* = {id} */: return "{name[4:]}";' for name, id in uhdm_types_h.get_type_map(models).items() ]

    # Serializer.h
    with open(config.get_template_filepath('Serializer.h'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<FACTORY_DATA_MEMBERS>', '\n'.join(factory_data_members))
    file_content = file_content.replace('<FACTORY_FUNCTION_DECLARATIONS>', '\n'.join(factory_function_declarations))
    file_content = file_content.replace('<OBJECT_ACCESSOR_DECLARATIONS>', '\n'.join(sorted(object_accessor_declarations)))
    file_utils.set_content_if_changed(config.get_output_header_filepath('Serializer.h'), file_content)

    # Serializer.cpp
    with open(config.get_template_filepath('Serializer.cpp'), 'rt') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<CAPNP_ID>', '\n'.join(sorted(save_ids)))
    file_content = file_content.replace('<FACTORY_GC>', '\n'.join(sorted(factory_gc)))
    file_content = file_content.replace('<UHDM_NAME_MAP>', '\n'.join(uhdm_name_map))
    file_content = file_content.replace('<FACTORY_PURGE>', '\n'.join(sorted(factory_purge)))
    file_content = file_content.replace('<FACTORY_STATS>', '\n'.join(sorted(factory_stats)))
    file_content = file_content.replace('<FACTORY_ERASE_OBJECT>', '\n'.join(sorted(factory_erase_object)))
    file_content = file_content.replace('<OBJECT_ACCESSOR_IMPLEMENTATIONS>', '\n'.join(sorted(object_accessor_implementations)))
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
    file_content = file_content.replace('<FACTORY_FUNCTION_IMPLEMENTATIONS>', '\n'.join(factory_function_implementations))
    file_content = file_content.replace('<FACTORY_GET_OBJECT>', '\n'.join(sorted(factory_get_object)))
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
