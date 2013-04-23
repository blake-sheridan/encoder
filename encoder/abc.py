from encoder import _abc

class Encoder(_abc.Encoder):
    class CannotEncode(Exception):
        pass

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
    def FLOAT_INFINITY(self) -> str:
        raise CannotEncode(float('inf'))

    @property
    def FLOAT_NEGATIVE_INFINITY(self) -> str:
        raise CannotEncode(float('-inf'))

    @property
    def FLOAT_NAN(self) -> str:
        raise CannotEncode(float('nan'))
