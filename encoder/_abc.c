#include <Python.h>

#define BUFFER_INITIAL_LENGTH 1000
#define BUFFER_INCREMENT 1000
#define BUFFER_MAX_LENGTH 100000

/* TODO: calc these instead of fudging. */
const int SPRINTF_MAX_LONG_LONG_LENGTH = 30 + 1; /* +1 for /0 */
const int SPRINTF_MAX_DOUBLE_LENGTH = 50 + 1;    /* +1 for /0 */

#define _CHAR(c)                                  \
    if (self->length == self->length_max)         \
        if (_resize(self) == -1)                  \
            return -1;                            \
    self->buffer[self->length++] = c;

#define ENSURE_CAN_HOLD(__length)                                       \
    if (self->length + __length >= self->length_max)                    \
        if (_resize(self) == -1)                                        \
            return -1;

#define READY_CONST(member, attribute)                           \
    if (member == NULL)                                          \
        if (_populate(self, &member, attribute) == -1)           \
            return -1;

typedef struct {
    PyObject_HEAD
    char *buffer;
    int length;
    int length_max;
    PyObject *none;
    PyObject *bool_true;
    PyObject *bool_false;
    PyObject *float_infinity;
    PyObject *float_negative_infinity;
    PyObject *float_nan;
} Encoder;

PyDoc_STRVAR(Encoder_doc,
"TODO Encoder doc");

static PyObject* encode(Encoder *self, PyObject *o);
static PyObject* encode_bytes(Encoder *self, PyObject *o);

static int _append(Encoder *self, PyObject *o);
static int _append_int(Encoder *self, PyObject *o);
static int _append_float(Encoder *self, PyObject *o);
static int _append_str(Encoder *self, PyObject *o);
static int _append_bytes(Encoder *self, PyObject *o);
static int _append_fast_sequence(Encoder *self, PyObject *o);

static int _resize(Encoder *self);
static int _write(Encoder *self, char *string, int length);

static int
_populate(Encoder *self, PyObject **member, char *name)
{
    PyObject *str = PyObject_GetAttrString((PyObject*)self, name);

    if (str == NULL) {
        return -1;
    }

    PyObject *bytes = PyUnicode_AsEncodedString(str, NULL, NULL);

    Py_DECREF(str);

    if (bytes == NULL) {
        return -1;
    }

    *member = bytes;

    return 0;
}

static PyObject *
__new__(PyTypeObject *type, PyObject *args, PyObject **kwargs)
{
    Encoder *self = (Encoder *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }

    self->buffer = PyMem_Malloc(BUFFER_INITIAL_LENGTH);
    if (self->buffer == NULL) {
        return NULL;
    }

    self->length = 0;
    self->length_max = BUFFER_INITIAL_LENGTH;

    self->none = NULL;
    self->bool_true = NULL;
    self->bool_false = NULL;
    self->float_infinity = NULL;
    self->float_negative_infinity = NULL;
    self->float_nan = NULL;

    return (PyObject *)self;
}

static void
__del__(Encoder* self)
{
    PyMem_Free(self->buffer);

    Py_XDECREF(self->none);
    Py_XDECREF(self->bool_true);
    Py_XDECREF(self->bool_false);
    Py_XDECREF(self->float_infinity);
    Py_XDECREF(self->float_negative_infinity);
    Py_XDECREF(self->float_nan);

    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
encode(Encoder* self, PyObject *o)
{
    PyObject *bytes = encode_bytes(self, o);
    if (bytes == NULL) {
        return NULL;
    }

    return PyUnicode_FromEncodedObject(bytes, NULL, NULL);
}

static PyObject*
encode_bytes(Encoder *self, PyObject *o)
{
    if (self->length != 0) {
        PyErr_SetString(PyExc_RuntimeError, "encode while encode already in progress");
        return NULL;
    }

    PyObject *retval = NULL;

    if (_append(self, o) != -1) {
        retval = PyBytes_FromStringAndSize(self->buffer, self->length);
    }

    self->length = 0;

    return retval;
}

static int
_append(Encoder *self, PyObject *o)
{
    if (o == Py_None) {
        READY_CONST(self->none, "NONE");
        return _write(self, PyBytes_AS_STRING(self->none), PyBytes_GET_SIZE(self->none));
    }
    if (o == Py_True) {
        READY_CONST(self->bool_true, "BOOL_TRUE");
        return _write(self, PyBytes_AS_STRING(self->bool_true), PyBytes_GET_SIZE(self->bool_true));
    }
    if (o == Py_False) {
        READY_CONST(self->bool_false, "BOOL_FALSE");
        return _write(self, PyBytes_AS_STRING(self->bool_false), PyBytes_GET_SIZE(self->bool_false));
    }
    if (PyLong_Check(o)) {
        return _append_int(self, o);
    }
    if (PyFloat_Check(o)) {
        return _append_float(self, o);
    }
    if (PyUnicode_Check(o)) {
        return _append_str(self, o);
    }
    if (PyBytes_Check(o)) {
        return _append_bytes(self, o);
    }
    if (PySequence_Check(o)) {
        /* Must occur after str/bytes (and byte array?), which are sequences. */
        PyObject *checked = PySequence_Fast(o, "Expected list/tuple");
        if (checked == NULL) {
            return -1;
        }
        return _append_fast_sequence(self, checked);
    }

    PyErr_Format(PyExc_TypeError, "Do not know how to encode %s", o->ob_type->tp_name);
    return -1;
}

static int
_append_int(Encoder *self, PyObject *integer)
{
    long long as_long = PyLong_AS_LONG(integer);
    if (PyErr_Occurred() != NULL) {
        return -1;
    }

    ENSURE_CAN_HOLD(SPRINTF_MAX_LONG_LONG_LENGTH);

    int num_chars = sprintf(&self->buffer[self->length], "%Ld", as_long);
    if (num_chars < 0) {
        PyErr_SetString(PyExc_RuntimeError, "failure from sprintf on int");
        return -1;
    }

    self->length += num_chars;

    return 0;
}

static int
_append_float(Encoder *self, PyObject *f)
{
    double as_double = PyFloat_AS_DOUBLE(f);

    ENSURE_CAN_HOLD(SPRINTF_MAX_DOUBLE_LENGTH);

    int num_chars = sprintf(&self->buffer[self->length], "%f", as_double);
    if (num_chars < 0) {
        PyErr_SetString(PyExc_RuntimeError, "failure from sprintf on float");
        return -1;
    }

    self->length += num_chars;

    /* Trim trailing 0's - does not seem to be way to tell sprintf to do so. */
    while (self->buffer[self->length - 1] == '0') {
        self->length--;
    }

    return 0;
}

static int
_append_str(Encoder *self, PyObject *s)
{
    if (PyUnicode_READY(s) == -1) {
        return -1;
    }

    Py_ssize_t length = PyUnicode_GET_LENGTH(s);

    _CHAR('"');

    if (length != 0) {
        PyErr_SetString(PyExc_NotImplementedError, "non-empty strings");
        return -1;
    }

    _CHAR('"');

    return 0;
}

static int
_append_bytes(Encoder *self, PyObject *bytes)
{
    PyErr_SetString(PyExc_NotImplementedError, "bytes");
    return -1;
}

static int
_append_fast_sequence(Encoder *self, PyObject *sequence)
{
    /* XXX: must be list/tuple, using the assumption macros */
    int length = PySequence_Fast_GET_SIZE(sequence);

    _CHAR('[');

    if (length != 0) {
        PyObject **items = PySequence_Fast_ITEMS(sequence);

        int i;

        for (i = 0; i < length; i++) {
            if (i != 0) {
                _CHAR(',');
            }
            _append(self, items[i]);
        }
    }

    _CHAR(']');

    return 0;
}

static int
_resize(Encoder *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "resizing internal buffer");
    return -1;
}

static int
_write(Encoder *self, char *string, int length)
{
    ENSURE_CAN_HOLD(length)

    int i;

    for (i = 0; i < length; i++) {
        self->buffer[self->length + i] = string[i];
    }

    self->length += length;

    return 0;
}

/*
static PyMethodDef Encoder_members[] = {
};
*/

static PyMethodDef Encoder_methods[] = {
    {"encode",       (PyCFunction)encode,       METH_O, "Encode an object as a string"},
    {"encode_bytes", (PyCFunction)encode_bytes, METH_O, "Encode an object as bytes"},
    {NULL} /* Sentinel */
};

static PyTypeObject EncoderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "encoder.Encoder",         /* tp_name */
    sizeof(Encoder),           /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)__del__,       /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
    Encoder_doc,               /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Encoder_methods,           /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    __new__,                   /* tp_new */
};

static struct PyModuleDef encoder_module = {
    PyModuleDef_HEAD_INIT,
    "_encoder",
    "todo module doc",
    -1,
    Encoder_methods
};

PyMODINIT_FUNC
PyInit__abc(void)
{
    if (PyType_Ready(&EncoderType) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&encoder_module);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&EncoderType);
    PyModule_AddObject(module, "Encoder", (PyObject *)&EncoderType);

    return module;
};
