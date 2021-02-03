# Copyright 2019-2020 Alain Dargelas
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import config
import file_utils
import VpiListener_h

def generate(models):
    methods = VpiListener_h.get_methods(models, ' TRACE_ENTER; ', ' TRACE_LEAVE; ')

    with open(config.get_template_filepath('VpiListenerTracer.h'), 'r+t') as strm:
        file_content = strm.read()

    file_content = file_content.replace('<VPI_LISTENER_TRACER_METHODS>', '\n'.join(methods))
    file_utils.set_content_if_changed(config.get_header_filepath('VpiListenerTracer.h'), file_content)
    return True


def _main():
    import loader

    config.set_cwd()

    models = loader.load_models()
    return generate(models)


if __name__ == '__main__':
    import sys
    sys.exit(0 if _main() else 1)
