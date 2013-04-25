#include <Python.h>

#define BUFFER_INITIAL_LENGTH 1000
#define BUFFER_INCREMENT 1000
#define BUFFER_MAX_LENGTH 100000

/* TODO: calc these instead of fudging. */

const int SPRINTF_MAX_LONG_LONG_LENGTH = 30 + 1; /* +1 for /0 */
const int SPRINTF_MAX_DOUBLE_LENGTH = 50 + 1;    /* +1 for /0 */

/* These macros must be used with Encoder `self` in scope, and return -1 if errors. */

#define WRITE_CHAR(__c)                               \
    if (_write_char(self, __c) == -1)                 \
        return -1;

#define WRITE_CONST(__s)                                                \
    if (_write_const(self, __s) == -1)                                  \
        return -1;

#define ENSURE_CAN_HOLD(__length)                                       \
    if (self->length + __length >= self->length_max)                    \
        if (_resize(self) == -1)                                        \
            return -1;

#define READY_BYTES_CONSTANT(member, attribute)                  \
    if (member == NULL)                                          \
        if (_populate_bytes_constant(self, &member, attribute) == -1)           \
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
    PyObject *string_escapes;
    PyObject *str_translation_table;
} Encoder;

PyDoc_STRVAR(Encoder_doc,
"TODO Encoder doc");

static PyObject* encode(Encoder *self, PyObject *o);
static PyObject* encode_bytes(Encoder *self, PyObject *o);

static int _append(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_int(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_float(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_str(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_str_1byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_2byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_4byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_naive(Encoder *self, PyObject *o, int length);

Py_LOCAL_INLINE(int) _append_bytes(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_fast_sequence(Encoder *self, PyObject *o);

Py_LOCAL_INLINE(int) _populate_bytes_constant(Encoder *self, PyObject **member, char *name);
Py_LOCAL_INLINE(int) _populate_string_escapes(Encoder *self);
Py_LOCAL_INLINE(int) _populate_str_translation_table(Encoder *self);

static int _resize(Encoder *self);

Py_LOCAL_INLINE(int) _write_char(Encoder *self, char c);
Py_LOCAL_INLINE(int) _write_const(Encoder *self, const char *string);
Py_LOCAL_INLINE(int) _write_bytes(Encoder *self, PyObject *bytes);

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

    self->string_escapes = NULL;
    self->str_translation_table = NULL;

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

    Py_XDECREF(self->string_escapes);
    Py_XDECREF(self->str_translation_table);

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
        READY_BYTES_CONSTANT(self->none, "NONE");
        return _write_bytes(self, self->none);
    }
    if (o == Py_True) {
        READY_BYTES_CONSTANT(self->bool_true, "BOOL_TRUE");
        return _write_bytes(self, self->bool_true);
    }
    if (o == Py_False) {
        READY_BYTES_CONSTANT(self->bool_false, "BOOL_FALSE");
        return _write_bytes(self, self->bool_false);
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

Py_LOCAL_INLINE(int)
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

Py_LOCAL_INLINE(int)
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

Py_LOCAL_INLINE(int)
_append_str(Encoder *self, PyObject *s)
{
    if (PyUnicode_READY(s) == -1) {
        return -1;
    }

    Py_ssize_t length = PyUnicode_GET_LENGTH(s);

    if (length == 0) {
        return _write_const(self, "\"\"");
    }

    WRITE_CHAR('"');

    switch (PyUnicode_KIND(s)) {
    case PyUnicode_1BYTE_KIND: {
        if (_append_str_1byte_kind(self, s, length) == -1) {
            return -1;
        }
        break;
    }
    case PyUnicode_2BYTE_KIND: {
        if (_append_str_2byte_kind(self, s, length) == -1) {
            return -1;
        }
        break;
    }
    default: {
        if (_append_str_4byte_kind(self, s, length) == -1) {
            return -1;
        }
    }
    }

    WRITE_CHAR('"');

    return 0;
}

Py_LOCAL_INLINE(int)
_append_str_1byte_kind(Encoder *self, PyObject *s, int length)
{
    /* TEMP: use correct, but slow, version. */
    return _append_str_naive(self, s, length);

    Py_UCS1 *data = PyUnicode_1BYTE_DATA(s);
    int i;

    for (i = 0; i < length; i++) {
        /* TODO: map string_escapes */
        if (_write_char(self, data[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

Py_LOCAL_INLINE(int)
_append_str_2byte_kind(Encoder *self, PyObject *s, int length)
{
    return _append_str_naive(self, s, length);
}

Py_LOCAL_INLINE(int)
_append_str_4byte_kind(Encoder *self, PyObject *s, int length)
{
    return _append_str_naive(self, s, length);
}

Py_LOCAL_INLINE(int)
_append_str_naive(Encoder *self, PyObject *s, int length)
{
    /* Handle STRING_ESCAPES correctly for any unicode,
       but 2x as slow as json_encode_basestring_ascii when I measured. */

    PyObject *translated = NULL;
    PyObject *translated_bytes = NULL;
    int retval = -1;

    if (self->str_translation_table == NULL) {
        if (_populate_str_translation_table(self) == -1) {
            goto bail;
        }
    }

    translated = PyUnicode_Translate(s, self->str_translation_table, NULL);
    if (translated == NULL) {
        goto bail;
    }

    translated_bytes = PyUnicode_AsEncodedString(translated, NULL, NULL);
    if (translated_bytes == NULL) {
        goto bail;
    }

    if (_write_bytes(self, translated_bytes) == -1) {
        goto bail;
    }

    retval = 0;

  bail:
    Py_XDECREF(translated);
    Py_XDECREF(translated_bytes);

    return retval;
}

Py_LOCAL_INLINE(int)
_append_bytes(Encoder *self, PyObject *bytes)
{
    PyErr_SetString(PyExc_NotImplementedError, "bytes");
    return -1;
}

Py_LOCAL_INLINE(int)
_append_fast_sequence(Encoder *self, PyObject *sequence)
{
    /* XXX: must be list/tuple, using the assumption macros */
    int length = PySequence_Fast_GET_SIZE(sequence);

    WRITE_CHAR('[');

    if (length != 0) {
        PyObject **items = PySequence_Fast_ITEMS(sequence);

        int i;

        for (i = 0; i < length; i++) {
            if (i != 0) {
                WRITE_CHAR(',');
            }
            _append(self, items[i]);
        }
    }

    WRITE_CHAR(']');

    return 0;
}


Py_LOCAL_INLINE(int)
_populate_bytes_constant(Encoder *self, PyObject **member, char *name)
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

Py_LOCAL_INLINE(int)
_populate_string_escapes(Encoder *self)
{
    int retval = -1;

    /* Borrowed references */
    PyObject *key, *value;

    /* Temporary references */
    PyObject *key_bytes = NULL;
    PyObject *value_bytes = NULL;
    PyObject *user_string_escapes = PyObject_GetAttrString((PyObject*)self, "STRING_ESCAPES");
    if (user_string_escapes == NULL) {
        goto bail;
    }

    if (!PyDict_Check(user_string_escapes)) {
        PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: expected dict, got: %s", Py_TYPE(user_string_escapes)->tp_name);
        goto bail;
    }

    /* Must decref on error. */
    self->string_escapes = PyDict_New();
    if (self->string_escapes == NULL) {
        goto bail;
    }

    Py_ssize_t pos = 0;

    while (PyDict_Next(user_string_escapes, &pos, &key, &value)) {
        if (!PyUnicode_Check(key) || !PyUnicode_Check(value)) {
            PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: expected dict[str] -> str, got item (%s, %s)",
                         Py_TYPE(key)->tp_name, Py_TYPE(value)->tp_name);
            goto bail;
        }


        key_bytes = PyUnicode_AsEncodedString(key, NULL, NULL);
        if (key_bytes == NULL) {
            goto bail;
        }

        if (PyBytes_GET_SIZE(key_bytes) != 1) {
            PyErr_Format(PyExc_ValueError, "STRING_ESCAPES: keys must be single characters, got: %s",
                         PyBytes_AS_STRING(key_bytes));
            goto bail;
        }

        value_bytes = PyUnicode_AsEncodedString(value, NULL, NULL);
        if (value_bytes == NULL) {
            goto bail;
        }


        if (PyDict_SetItem(self->string_escapes, key_bytes, value_bytes) == -1) {
            goto bail;
        }


        Py_DECREF(key_bytes);
        Py_DECREF(value_bytes);
    }

    retval = 0;

  bail:
    Py_XDECREF(user_string_escapes);
    Py_XDECREF(key_bytes);
    Py_XDECREF(value_bytes);

    if (retval == -1) {
        Py_XDECREF(self->string_escapes);
    }

    return retval;
}

Py_LOCAL_INLINE(int)
_populate_str_translation_table(Encoder *self)
{
    PyObject *user_string_escapes = PyObject_GetAttrString((PyObject*)self, "STRING_ESCAPES");
    if (user_string_escapes == NULL) {
        return -1;
    }

    PyObject *s = PyUnicode_New(0, 127); //FromStringAndSize("", 0);
    if (s == NULL) {
        return -1;
    }

    PyObject *maketrans = PyObject_GetAttrString(s, "maketrans");
    Py_DECREF(s);
    if (maketrans == NULL) {
        return -1;
    }

    self->str_translation_table = PyObject_CallFunctionObjArgs(maketrans, user_string_escapes, NULL);


    if (self->str_translation_table == NULL) {
        return -1;
    }

    return 0;
}

static int
_resize(Encoder *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "resizing internal buffer");
    return -1;
}

Py_LOCAL_INLINE(int)
_write_char(Encoder *self, const char c)
{
    if (self->length == self->length_max) {
        if (_resize(self) == -1) {
            return -1;
        }
    }

    self->buffer[self->length++] = c;

    return 0;
}

Py_LOCAL_INLINE(int)
_write_const(Encoder *self, const char *c)
{
    if (self->length + strlen(c) >= self->length_max) {
        if (_resize(self) == -1) {
            return -1;
        }
    }

    strncpy(self->buffer, c, strlen(c));

    self->length += strlen(c);

    return 0;
}

Py_LOCAL_INLINE(int)
_write_bytes(Encoder *self, PyObject *bytes)
{
    int length = PyBytes_GET_SIZE(bytes);
    char *string = PyBytes_AS_STRING(bytes);

    ENSURE_CAN_HOLD(length);

    strncpy(&(self->buffer)[self->length], string, length);

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
