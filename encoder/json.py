from . import abc

class Encoder(abc.Encoder):
    NONE = 'null'
    BOOL_TRUE = 'true'
    BOOL_FALSE = 'false'
    FLOAT_INFINITY = 'Infinity'
    FLOAT_NEGATIVE_INFINITY = '-Infinity'
    FLOAT_NAN = 'NaN'

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
