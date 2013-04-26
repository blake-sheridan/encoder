import unittest

# temp
import sys
sys.path.append('build/lib.linux-x86_64-3.3')

import encoder.json

class JsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.encode = encoder.json.Encoder().encode

    def check(self, o:object, s:str):
        """Assert `o` encodes to `s`, regardless of non-significant whitespace."""
        encoded = self.encode(o)
        encoded_stripped = encoded.replace(' ', '') # TODO: tabs/newlines when I need

        s_stripped = s.replace(' ', '')

        self.assertEqual(encoded_stripped, s_stripped)

    def test_None(self):
        self.check(None, 'null')

    def test_True(self):
        self.check(True, 'true')

    def test_False(self):
        self.check(False, 'false')

    def test_int(self):
        self.check(2, '2')

    def test_float(self):
        self.check(2.5, '2.5')

    def test_str_empty(self):
        self.check("", '""')

    def test_str_literal(self):
        self.check('abc', '"abc"')

    def test_str_escape(self):
        self.check("a\"bc", '"a\\"bc"')

    def test_list(self):
        self.check([1, 2, 3], '[1, 2, 3]')

    def test_dict(self):
        # This one's harder to check, due to dict randomization

        DICT = {'a': True, 'b': False, 'c': None}

        encoded = self.encode(DICT)

        evalable = encoded.replace('true', 'True').replace('false', 'False').replace('null', 'None')

        self.assertEqual(DICT, eval(evalable))

    def test_dict_preserve_order(self):
        # Keep test agnostic as to spaces after commas.
        # The exact mechanism for triggering this behavior is subject to change.
        from collections import OrderedDict

        d = OrderedDict()
        s_expected = '{'

        first = True
        # Enough items to make a coincidence unlikely
        for key, value, encoded in [
            ('a', True, 'true'),
            ('b', False, 'false'),
            ('c', None, 'null'),
            ('d', 1, '1'),
            ('e', 2, '2'),
            ('f', 3, '3'),
            ]:

            d[key] = value

            if first:
                first = False
                s_expected += '"{}": {}'.format(key, encoded)
            else:
                s_expected += ',"{}": {}'.format(key, encoded)

        s_expected += '}'

        self.check(d, s_expected)
