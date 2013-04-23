from . import abc

class Encoder(abc.Encoder):
    NONE = 'null'
    BOOL_TRUE = 'true'
    BOOL_FALSE = 'false'
    FLOAT_INFINITY = 'Infinity'
    FLOAT_NEGATIVE_INFINITY = '-Infinity'
    FLOAT_NAN = 'NaN'
