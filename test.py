import unittest

# temp
import sys
sys.path.append('build/lib.linux-x86_64-3.3')

import encoder.json

class JsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.encode = encoder.json.Encoder().encode

    def test_None(self):
        self.assertEqual(self.encode(None), 'null')

    def test_True(self):
        self.assertEqual(self.encode(True), 'true')

    def test_False(self):
        self.assertEqual(self.encode(False), 'false')

    def test_int(self):
        self.assertEqual(self.encode(2), '2')

    def test_float(self):
        self.assertEqual(self.encode(2.5), '2.5')

    def test_str_empty(self):
        self.assertEqual(self.encode(""), '""')

    def test_str_literal(self):
        self.assertEqual(self.encode("abc"), '"abc"')

    def test_str_escape(self):
        self.assertEqual(self.encode("a\"bc"), '"a\\"bc"')

    def test_list(self):
        # Keep test agnostic as to spaces after commas
        self.assertEqual(self.encode([1, 2, 3]), '[1, 2, 3]'.replace(' ', ''))
