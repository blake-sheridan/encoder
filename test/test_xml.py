import unittest

import encoder.xml

class XmlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.encode = encoder.xml.Encoder().encode

    def test_sample_doc(self):
        class SampleDoc:
            def __xml__(self, tag):
                with tag.body():
                    yield 'Hello world!'

        self.assertEqual(self.encode(SampleDoc()), '<body>Hello world!</body>')
