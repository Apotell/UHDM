import argparse
import concurrent.futures
import os

import config
import file_utils
import loader

import capnp
import class_hierarchy
import classes
import containers_h
import ElaboratorListener_cpp
import serializer
import uhdm_forward_decl_h
import uhdm_h
import uhdm_types_h
import UhdmListener
import UhdmListenerTracer_h
import vpi_user_cpp
import vpi_visitor
import VpiListener
import VpiListenerTracer_h


def _worker(params):
    key, args = params

    config.log(f'   ... {key}')

    if key == 'capnp':
        return capnp.generate(*args)

    elif key == 'class_hierarchy':
        return class_hierarchy.generate(*args)

    elif key == 'classes':
        return classes.generate(*args)

    elif key == 'containers_h':
        return containers_h.generate(*args)

    elif key == 'ElaboratorListener_cpp':
        return ElaboratorListener_cpp.generate(*args)

    elif key == 'serializer':
        return serializer.generate(*args)

    elif key == 'uhdm_forward_decl_h':
        return uhdm_forward_decl_h.generate(*args)

    elif key == 'uhdm_h':
        return uhdm_h.generate(*args)

    elif key == 'uhdm_types_h':
        return uhdm_types_h.generate(*args)

    elif key == 'UhdmListener':
      return UhdmListener.generate(*args)

    elif key == 'UhdmListenerTracer_h':
      return UhdmListenerTracer_h.generate(*args)

    elif key == 'vpi_user_cpp':
        return vpi_user_cpp.generate(*args)

    elif key == 'vpi_visitor':
        return vpi_visitor.generate(*args)

    elif key == 'VpiListener':
        return VpiListener.generate(*args)

    elif key == 'VpiListenerTracer_h':
        return VpiListenerTracer_h.generate(*args)

    config.log('ERROR: Unknown key "{key}"')
    return False


def _main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-output-dirpath', dest='output_dirpath', type=str, help='Output path')
    parser.add_argument('--parallel', dest='parallel', action='store_true', default=True)
    parser.add_argument('--source-dirpath', dest='source_dirpath', type=str, help='Path to UHDM source')
    args = parser.parse_args()
    config.configure(args)

    print('Generating UHDM models ...')
    print('   Loading models ...')
    models = loader.load_models()
    print(f'   ... found {len(models)} models.')

    print('   Validating ordering ...')
    # Check validity
    index = 0
    order = {}
    for name in models.keys():
        order[name] = index
        index += 1

    for name, model in models.items():
        baseclass = model['extends']
        if baseclass:
            thisIndex = order[name]
            baseIndex = order[baseclass]
            if baseIndex >= thisIndex:
                raise Exception(f'Model {name} should follow {baseclass} in listing.')
    print('   ... all good.')

    params = [
        ('capnp', [models]),
        ('class_hierarchy', [models]),
        ('classes', [models]),
        ('containers_h', [models]),
        ('ElaboratorListener_cpp', [models]),
        ('serializer', [models]),
        ('uhdm_forward_decl_h', [models]),
        ('uhdm_h', [models]),
        ('uhdm_types_h', [models]),
        ('UhdmListener', [models]),
        ('UhdmListenerTracer_h', [models]),
        ('vpi_user_cpp', [models]),
        ('vpi_visitor', [models]),
        ('VpiListener', [models]),
        ('VpiListenerTracer_h', [models]),
    ]

    if args.parallel:
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            results = list(executor.map(_worker, params))
    else:
        results = [_worker(args) for args in params]
    print('... all done!')

    result = sum([0 if r else 1 for r in results])
    if result:
        print('ERROR: UHDM model generation FAILED!')
    else:
        print('UHDM Models generated successfully!.')

    return result


if __name__ == '__main__':
    import sys
    sys.exit(_main())
