#!/usr/local/bin/python3

from distutils.core import setup, Extension

setup(
    name = 'Encoder',
    version = '1.0',
    description = 'Encode stuff',
    ext_modules = [
        Extension(
            name = '_encoder',
            sources = [
                'src/buffer.c',
                'src/encoder.c',
                'src/module.c',
                'src/xml.c',
                ],
            include_dirs = [
                'include',
                ],
            depends = [
                'include/buffer.h', # As this is essentially a source file
                ],
            ),
        ],
    )
