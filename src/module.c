#include <Python.h>

extern PyTypeObject Encoder_Type;
extern PyTypeObject Tag_Type;

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
    PyObject *module = PyModule_Create(&Module);
    if (module != NULL) {
        if (PyType_Ready(&Encoder_Type) < 0)
            return NULL;

        if (PyType_Ready(&Tag_Type) < 0)
            return NULL;

        Py_INCREF(&Encoder_Type);
        Py_INCREF(&Tag_Type);

        PyModule_AddObject(module, "Encoder", (PyObject *)&Encoder_Type);
        PyModule_AddObject(module, "Tag",     (PyObject *)&Tag_Type);
    }
    return module;
};
