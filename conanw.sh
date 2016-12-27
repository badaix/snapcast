#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ -f $DIR/.python/bin/activate ]
then
    source $DIR/.python/bin/activate
    conan "$@"
else
    python2 -c 'import virtualenv
virtualenv.main()' -p python2 $DIR/.python
    source $DIR/.python/bin/activate
    pip install conan_package_tools
    conan "$@"
    deactivate
fi
