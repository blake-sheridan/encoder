from encoder import _abc

class CannotEncode(Exception):
    pass

class Encoder(_abc.Encoder):
    @property
    def NONE(self) -> str:
        raise CannotEncode(None)

    @property
    def BOOL_TRUE(self) -> str:
        raise CannotEncode(True)

    @property
    def BOOL_FALSE(self) -> str:
        raise CannotEncode(False)

    @property
    def DICT_PRESERVE_ORDER(self) -> bool:
        return False

    @property
    def FLOAT_INFINITY(self) -> str:
        raise CannotEncode(float('inf'))

    @property
    def FLOAT_NEGATIVE_INFINITY(self) -> str:
        raise CannotEncode(float('-inf'))

    @property
    def FLOAT_NAN(self) -> str:
        raise CannotEncode(float('nan'))

    @property
    def STRING_ESCAPES(self) -> (dict, str, str):
        raise NotImplementedError
