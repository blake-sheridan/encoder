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
                'src/encoder.c',
                ],
            include_dirs = [
                'include',
                ],
            ),
        ],
    )
