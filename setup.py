#!/usr/bin/env python3
"""
An experimental programming language.

"""

import os
import io
import re
import setuptools


metapath = os.path.join(os.path.dirname(__file__), 'pyro', '__init__.py')
with io.open(metapath, encoding='utf-8') as metafile:
    regex = r'''^__([a-z]+)__ = ["'](.*)["']'''
    meta = dict(re.findall(regex, metafile.read(), flags=re.MULTILINE))


setuptools.setup(
    name = 'pyrolang',
    version = meta['version'],
    packages = setuptools.find_packages(),
    entry_points = {
        'console_scripts': [
            'pyro = pyro:main',
        ],
    },
    install_requires = [],
    python_requires = ">=3.8",
    author = 'Darren Mulholland',
    url='https://github.com/dmulholl/pyro',
    license = 'Public Domain',
    description = 'An experimental programming language.',
    long_description = __doc__,
    classifiers = [
        'Programming Language :: Python :: 3',
        'Operating System :: OS Independent',
        'License :: Public Domain',
    ],
)

