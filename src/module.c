#include <Python.h>

extern PyTypeObject EncoderType;

PyDoc_STRVAR(__doc__,
"TODO module __doc__");

static struct PyModuleDef Module = {
    PyModuleDef_HEAD_INIT,
    "_encoder",
    __doc__,
    -1,
};

PyMODINIT_FUNC
PyInit__encoder(void)
{
    if (PyType_Ready(&EncoderType) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&Module);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&EncoderType);
    PyModule_AddObject(module, "Encoder", (PyObject *)&EncoderType);

    return module;
};
