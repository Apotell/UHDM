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

# Set content of filename, but only if the file doesn't exist or the content
# is different than before.
# Return if content was overwritten.


import shutil
import os


def set_content_if_changed(filename, content):
    if os.path.exists(filename):
        with open(filename, 'r+t') as strm:
            orig_content = strm.read()

        if orig_content == content:
            return False

    with open(filename, 'w+t') as outfd:
        outfd.write(content)

    return True


def copy_file_if_changed(source, dest):
    '''
    Copy file from source to destination if destination does not exist or
    its content is different.
    '''
    source_content = None
    if os.path.exists(source):
        with open(source, 'r+t') as strm:
            source_content = strm.read()

    dest_content = None
    if os.path.exists(dest):
        with open(dest, 'r+t') as strm:
            dest_content = strm.read()

    if source_content and (source_content == dest_content):
        return False

    shutil.copyfile(source, dest)
    return True


def find_file(basedir, filename):
    for subdir, dirs, filenames in os.walk(basedir):
        if filename in filenames:
            return os.path.join(subdir, filename)

    return None

def remove_file_safely(filepath):
    if os.path.isfile(filepath):
        os.remove(filepath)
        return True

    return False
