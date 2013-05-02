import _encoder

class CannotEncode(Exception):
    pass

class Encoder(_encoder.Encoder):
    @property
    def NONE(self) -> str:
        raise CannotEncode(None)

    @property
    def TRUE(self) -> str:
        raise CannotEncode(True)

    @property
    def FALSE(self) -> str:
        raise CannotEncode(False)

    @property
    def DICT_PRESERVE_ORDER(self) -> bool:
        return False

    @property
    def INFINITY(self) -> str:
        raise CannotEncode(float('inf'))

    @property
    def NEGATIVE_INFINITY(self) -> str:
        raise CannotEncode(float('-inf'))

    @property
    def NAN(self) -> str:
        raise CannotEncode(float('nan'))

    @property
    def STRING_ESCAPES(self) -> (dict, str, str):
        raise NotImplementedError

    def make_iterencode(self, type:type):
        raise CannotEncode(type)
