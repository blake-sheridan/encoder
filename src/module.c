#include <Python.h>

extern PyTypeObject Encoder_Type;

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
    if (PyType_Ready(&Encoder_Type) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&Module);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&Encoder_Type);
    PyModule_AddObject(module, "Encoder", (PyObject *)&Encoder_Type);

    return module;
};
