import sys
sys.path.append('build/lib.linux-x86_64-3.3')

import _encoder

from . import abc

class _TagFactory:
    def __init__(self, encoder):
        self._encoder = encoder

    def __getattr__(self, name):
        value = self.__dict__[name] = _encoder.Tag(self._encoder, name)
        return value

class Encoder(abc.Encoder):
    TRUE = '1'
    FALSE = '0'

    STRING_ESCAPES = {
        '<': '&lt;',
        '>': '&gt;',
        '&': '&amp;',
        }

    def __init__(self):
        self.tag = _TagFactory(self)

    def make_iterencode(self, type):
        if hasattr(type, '__xml__'):
            return type.__xml__, self.tag
        else:
            return super().make_iterencode(type)
