#!/usr/local/bin/python3

from distutils.core import setup, Extension

setup(
    name = 'Encoder',
    version = '1.0',
    description = 'Encode stuff',
    ext_modules = [
        Extension('encoder._abc',
                  sources = ['encoder/_abc.c']),
        ],
    )
