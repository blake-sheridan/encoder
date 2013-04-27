#include <Python.h>

#define BUFFER_INITIAL_LENGTH 1024
#define BUFFER_INCREMENT 1024
#define BUFFER_MAX_LENGTH 10240

/* TODO: calc these instead of fudging. */

const int SPRINTF_MAX_LONG_LONG_LENGTH = 30 + 1; /* +1 for /0 */
const int SPRINTF_MAX_DOUBLE_LENGTH = 50 + 1;    /* +1 for /0 */

const char QUOTE = '"';

/* These macros must be used with Encoder `self` in scope and ASSUME length has been checked. */

#define CHAR_UNSAFE(__char)                            \
    self->buffer[self->length++] = __char;

#define STRING_UNSAFE(__string, __length)                              \
    strncpy(&(self->buffer)[self->length], __string, __length); \
    self->length += __length;

/* These macros must be used with Encoder `self` in scope, and return -1 if errors. */

#define ENSURE_CAN_HOLD(__length)                                       \
    if (self->length + __length >= self->length_max)                    \
        if (_resize(self) == -1)                                        \
            return -1;

#define CHAR(__char)                            \
    if (self->length == self->length_max)       \
        if (_resize(self) == -1)                \
            return -1;                          \
    CHAR_UNSAFE(__char);

#define STRING(__string, __length)                \
    ENSURE_CAN_HOLD(__length);                    \
    STRING_UNSAFE(__string, __length);

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

    int dict_preserve_order;

    PyObject *_str_translation_table;
    Py_UCS1 **_str_ucs1_mapping;
} Encoder;

PyDoc_STRVAR(Encoder___doc__,
"TODO Encoder __doc__");

static PyObject* encode(Encoder *self, PyObject *o);

PyDoc_STRVAR(encode___doc__,
"TODO encode __doc__");

static PyObject* encode_bytes(Encoder *self, PyObject *o);

PyDoc_STRVAR(encode_bytes___doc__,
"TODO encode_bytes __doc__");

static int _append(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_int(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_float(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_str(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_str_1byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_2byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_4byte_kind(Encoder *self, PyObject *o, int length);
Py_LOCAL_INLINE(int) _append_str_naive(Encoder *self, PyObject *o, int length);

Py_LOCAL_INLINE(int) _append_bytes(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_dict(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_fast_sequence(Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_mapping(Encoder *self, PyObject *o);

Py_LOCAL_INLINE(PyObject*)  _get_str_translation_table(Encoder *self);
Py_LOCAL_INLINE(Py_UCS1 **) _get_str_ucs1_mapping(Encoder *self);


static int _resize(Encoder *self);

Py_LOCAL_INLINE(int) _write_bytes(Encoder *self, PyObject *bytes);
Py_LOCAL_INLINE(int) _write_bytes_constant(Encoder *self, PyObject **member, const char *attrname);

static void _xfree_str_ucs1_mapping(Encoder *self);

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

    self->dict_preserve_order = -1;

    self->_str_translation_table = NULL;
    self->_str_ucs1_mapping = NULL;

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

    Py_XDECREF(self->_str_translation_table);

    _xfree_str_ucs1_mapping(self);

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
        return _write_bytes_constant(self, &self->none, "NONE");
    }
    if (o == Py_True) {
        return _write_bytes_constant(self, &self->bool_true, "BOOL_TRUE");
    }
    if (o == Py_False) {
        return _write_bytes_constant(self, &self->bool_false, "BOOL_FALSE");
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
    if (PyDict_Check(o)) {
        return _append_dict(self, o);
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
        STRING("\"\"", 2);
        return 0;
    }

    switch (PyUnicode_KIND(s)) {
    case PyUnicode_1BYTE_KIND: {
        return _append_str_1byte_kind(self, s, length);
    }
    case PyUnicode_2BYTE_KIND: {
        return _append_str_2byte_kind(self, s, length);
    }
    default: {
        return _append_str_4byte_kind(self, s, length);
    }
    }

    return 0;
}

Py_LOCAL_INLINE(int)
_append_str_1byte_kind(Encoder *self, PyObject *s, int slen)
{
    Py_UCS1 **mapping = _get_str_ucs1_mapping(self);
    if (mapping == NULL) {
        return -1;
    }

    Py_UCS1 *data = PyUnicode_1BYTE_DATA(s);
    PyObject *sub; /* bytes substitution */
    int i;
    int index_written = -1;

    for (i = 0; i < slen; i++) {
        sub = mapping[data[i]];

        if (sub != NULL) {
            if (index_written == -1) {
                CHAR(QUOTE);

                if (i != 0) {
                    STRING((char *)data, i);
                }
            }
            else {
                if (i != 0) {
                    STRING(&data[index_written + 1], i - index_written);
                }
            }

            if (_write_bytes(self, sub) == -1) {
                return -1;
            }

            index_written = i;
        }
    }

    if (index_written == -1) {
        /* The best case, where no subs occured. */
        ENSURE_CAN_HOLD(slen + 2);

        CHAR_UNSAFE(QUOTE);
        STRING_UNSAFE((char *)data, slen);
        CHAR_UNSAFE(QUOTE);
    }
    else {
        if (index_written != slen - 1) {
            STRING(&data[index_written + 1], slen - index_written - 1);
        }

        CHAR(QUOTE);
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

    PyObject *translation_table = _get_str_translation_table(self);
    if (translation_table == NULL) {
        goto bail;
    }

    translated = PyUnicode_Translate(s, translation_table, NULL);
    if (translated == NULL) {
        goto bail;
    }

    translated_bytes = PyUnicode_AsEncodedString(translated, NULL, NULL);
    if (translated_bytes == NULL) {
        goto bail;
    }

    CHAR(QUOTE);

    if (_write_bytes(self, translated_bytes) == -1) {
        goto bail;
    }

    CHAR(QUOTE);

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
_append_dict(Encoder *self, PyObject *dict)
{
    PyObject *key;
    PyObject *value;


    if (self->dict_preserve_order == -1) {
        PyObject *user_dict_preserve_order = PyObject_GetAttrString((PyObject*)self, "DICT_PRESERVE_ORDER");
        if (user_dict_preserve_order == NULL) {
            return -1;
        }
        self->dict_preserve_order = PyObject_IsTrue(user_dict_preserve_order);

        Py_DECREF(user_dict_preserve_order);

        if (self->dict_preserve_order == -1) {
            return -1;
        }
    }

    if (!PyDict_CheckExact(dict) && self->dict_preserve_order == 1) {
        return _append_mapping(self, dict);
    }

    Py_ssize_t pos = 0; /* NOT incremental */
    int index = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        if (index == 0) {
            CHAR('{');
        }
        else {
            CHAR(',');
        }

        _append(self, key);
        CHAR(':');
        _append(self, value);

        index++;
    }

    if (index == 0) {
        /* Empty. */
        STRING("{}", 2);
    } else {
        CHAR('}');
    }

    return 0;
}

Py_LOCAL_INLINE(int)
_append_mapping(Encoder *self, PyObject *mapping) {
    PyObject *items = PyMapping_Items(mapping);

    if (items == NULL) {
        return -1;
    }

    int length = PySequence_Fast_GET_SIZE(items);

    if (length == 0) {
        STRING("{}", 2);
    }
    else {
        /* Borrowed references */
        PyObject *item;

        int i;

        CHAR('{');

        for (i = 0; i < length; i++) {
            if (i != 0) {
                CHAR(',');
            }

            item = PyList_GET_ITEM(items, i);

            _append(self, PyTuple_GET_ITEM(item, 0));
            CHAR(':');
            _append(self, PyTuple_GET_ITEM(item, 1));
        }

        CHAR('}');
    }

    Py_DECREF(items);

    return 0;
}

Py_LOCAL_INLINE(int)
_append_fast_sequence(Encoder *self, PyObject *sequence)
{
    /* XXX: must be list/tuple, using the assumption macros */
    int length = PySequence_Fast_GET_SIZE(sequence);

    if (length == 0) {
        STRING("[]", 2);
    }
    else {
        PyObject **items = PySequence_Fast_ITEMS(sequence);

        int i;

        CHAR('[');

        for (i = 0; i < length; i++) {
            if (i != 0) {
                CHAR(',');
            }
            _append(self, items[i]);
        }

        CHAR(']');
    }

    return 0;
}


Py_LOCAL_INLINE(Py_UCS1**)
_get_str_ucs1_mapping(Encoder *self)
{
    if (self->_str_ucs1_mapping != NULL) {
        return self->_str_ucs1_mapping;
    }

    /* Temporary references */
    PyObject *user_string_escapes = NULL;

    /* Borrowed references */
    PyObject *key, *value;

    /* Kept references - dec on __del__ */
    PyObject *value_bytes;

    user_string_escapes = PyObject_GetAttrString((PyObject*)self, "STRING_ESCAPES");
    if (user_string_escapes == NULL) {
        goto error;
    }

    if (!PyDict_Check(user_string_escapes)) {
        PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: expected dict, got: %s", Py_TYPE(user_string_escapes)->tp_name);
        goto error;
    }

    self->_str_ucs1_mapping = PyMem_Malloc(sizeof(Py_UCS1*) * 256);
    if (self->_str_ucs1_mapping == NULL) {
        goto error;
    }

    int i;
    for (i = 0; i < 256; i++) {
        self->_str_ucs1_mapping[i] = NULL;
    }

    Py_ssize_t pos = 0;

    while (PyDict_Next(user_string_escapes, &pos, &key, &value)) {
        if (!PyUnicode_Check(key) || !PyUnicode_Check(value)) {
            PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: expected dict[str] -> str, got item (%s, %s)",
                         Py_TYPE(key)->tp_name, Py_TYPE(value)->tp_name);
            goto error;
        }

        if (PyUnicode_READY(key) == -1) {
            goto error;
        }

        if (PyUnicode_KIND(key) != PyUnicode_1BYTE_KIND) {
            /* TODO: test this path */
            PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: keys must be UCS1, got: %R", key);
            goto error;
        }

        if (PyUnicode_GET_LENGTH(key) != 1) {
            PyErr_Format(PyExc_TypeError, "STRING_ESCAPES: keys must exactly one character, got: %R", key);
            goto error;
        }

        value_bytes = PyUnicode_AsEncodedString(value, NULL, NULL);
        if (value_bytes == NULL) {
            goto error;
        }

        Py_UCS1 offset = PyUnicode_1BYTE_DATA(key)[0];

        self->_str_ucs1_mapping[offset] = value_bytes;
    }

    goto cleanup;
  error:
    _xfree_str_ucs1_mapping(self);
  cleanup:
    Py_XDECREF(user_string_escapes);
    return self->_str_ucs1_mapping;
}

Py_LOCAL_INLINE(PyObject*)
_get_str_translation_table(Encoder *self)
{
    if (self->_str_translation_table == NULL) {
        PyObject *user_string_escapes = PyObject_GetAttrString((PyObject*)self, "STRING_ESCAPES");
        if (user_string_escapes == NULL) {
            return NULL;
        }

        PyObject *s = PyUnicode_New(0, 127); //FromStringAndSize("", 0);
        if (s == NULL) {
            return NULL;
        }

        PyObject *maketrans = PyObject_GetAttrString(s, "maketrans");
        Py_DECREF(s);
        if (maketrans == NULL) {
            return NULL;
        }

        self->_str_translation_table = PyObject_CallFunctionObjArgs(maketrans, user_string_escapes, NULL);
    }

    return self->_str_translation_table;;
}

static int
_resize(Encoder *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "resizing internal buffer");
    return -1;
}

Py_LOCAL_INLINE(int)
_write_bytes_constant(Encoder *self, PyObject **member, const char *name)
{
    if (*member == NULL) {
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
    }

    return _write_bytes(self, *member);
}

Py_LOCAL_INLINE(int)
_write_bytes(Encoder *self, PyObject *bytes)
{
    STRING(PyBytes_AS_STRING(bytes), PyBytes_GET_SIZE(bytes));
    return 0;
}

static void
_xfree_str_ucs1_mapping(Encoder *self) {
    if (self->_str_ucs1_mapping != NULL) {
        int i;

        for (i = 0; i < 256; i++) {
            Py_XDECREF(self->_str_ucs1_mapping[i]);
        }
        PyMem_Free(self->_str_ucs1_mapping);

        self->_str_ucs1_mapping = NULL;
    }
};

static PyMethodDef Encoder_methods[] = {
    {"encode",         (PyCFunction)encode,         METH_O, encode___doc__},
    {"encode_bytes",   (PyCFunction)encode_bytes,   METH_O, encode_bytes___doc__},
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
    Encoder___doc__,           /* tp_doc */
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
PyInit__encoder(void)
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
