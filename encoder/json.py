from . import abc

class Encoder(abc.Encoder):
    NONE = 'null'
    TRUE = 'true'
    FALSE = 'false'
    INFINITY = 'Infinity'
    NEGATIVE_INFINITY = '-Infinity'
    NAN = 'NaN'

    DICT_PRESERVE_ORDER = True

    @property
    def STRING_ESCAPES(self):
        # See: json.encoder
        escapes = {
            '\\': '\\\\',
            '"': '\\"',
            '\b': '\\b',
            '\f': '\\f',
            '\n': '\\n',
            '\r': '\\r',
            '\t': '\\t',
            }

        for i in range(0x20):
            escapes.setdefault(chr(i), '\\u{0:04x}'.format(i))

        return escapes
