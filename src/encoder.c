#include <Python.h>

#include "buffer.h"
#include "encoder.h"

/* Forward declarations */
static PyObject* encode                     (Encoder *self, PyObject *o);
static PyObject* encode_bytes               (Encoder *self, PyObject *o);

static int           _append                (Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_bytes          (Encoder *self, PyObject *bytes);
Py_LOCAL_INLINE(int) _append_dict           (Encoder *self, PyObject *dict);
Py_LOCAL_INLINE(int) _append_int            (Encoder *self, PyObject *py_int);
Py_LOCAL_INLINE(int) _append_fast_sequence  (Encoder *self, PyObject *list_or_tuple);
Py_LOCAL_INLINE(int) _append_float          (Encoder *self, PyObject *py_float);
Py_LOCAL_INLINE(int) _append_mapping        (Encoder *self, PyObject *mapping);

Py_LOCAL_INLINE(int) _append_str            (Encoder *self, PyObject *str);
Py_LOCAL_INLINE(int) _append_str_1byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_2byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_4byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_naive      (Encoder *self, PyObject *str, int length);

Py_LOCAL_INLINE(int) _append_bytes_constant (Encoder *self, PyObject **member, const char *attribute_name);

/*
 * Lazy initialization accessors.
 * Should be the only means of getting their respective attributes.
 */
Py_LOCAL_INLINE(PyObject*)  _get_str_translation_table (Encoder *self);
Py_LOCAL_INLINE(Py_UCS1 **) _get_str_ucs1_mapping      (Encoder *self);

static void _xfree_str_ucs1_mapping(Encoder *self);

PyDoc_STRVAR(__doc__,
"TODO Encoder __doc__");

PyDoc_STRVAR(encode___doc__,
"TODO encode __doc__");

PyDoc_STRVAR(encode_bytes___doc__,
"TODO encode_bytes __doc__");

static PyObject *
__new__(PyTypeObject *type, PyObject *args, PyObject **kwargs)
{
    Encoder *self = (Encoder *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }

    self->buffer = new_buffer();
    if (self->buffer == NULL) {
        return NULL;
    }

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
    delete_buffer(self->buffer);

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
    /* FIXME: _index */

    if (self->buffer->_index != 0) {
        PyErr_SetString(PyExc_RuntimeError, "encode while encode already in progress");
        return NULL;
    }

    PyObject *retval = NULL;

    if (_append(self, o) != -1) {
        retval = Buffer_as_bytes(self->buffer);
    }

    self->buffer->_index = 0;

    return retval;
}

static int
_append(Encoder *self, PyObject *o)
{
    if (o == Py_None) {
        return _append_bytes_constant(self, &self->none, "NONE");
    }
    if (o == Py_True) {
        return _append_bytes_constant(self, &self->bool_true, "TRUE");
    }
    if (o == Py_False) {
        return _append_bytes_constant(self, &self->bool_false, "FALSE");
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

    return append_longlong(self->buffer, as_long);
}

Py_LOCAL_INLINE(int)
_append_float(Encoder *self, PyObject *f)
{
    return append_double(self->buffer, PyFloat_AS_DOUBLE(f));
}

Py_LOCAL_INLINE(int)
_append_str(Encoder *self, PyObject *s)
{
    if (PyUnicode_READY(s) == -1) {
        return -1;
    }

    Py_ssize_t length = PyUnicode_GET_LENGTH(s);

    if (length == 0) {
        return append_string(self->buffer, "\"\"", 2);
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
                if (append_char(self->buffer, '"') == -1) {
                    return -1;
                }

                if (i != 0) {
                    if (append_string(self->buffer, (char *)data, i) == -1) {
                        return -1;
                    }
                }
            }
            else {
                if (i != 0) {
                    if (append_string(self->buffer, &data[index_written + 1], i - index_written) == -1) {
                        return -1;
                    }
                }
            }

            if (append_bytes(self->buffer, sub) == -1) {
                return -1;
            }

            index_written = i;
        }
    }

    if (index_written == -1) {
        /* The best case, where no subs occured. */
        if (ensure_room(self->buffer, slen + 2) == -1) {
            return -1;
        }

        append_char_unsafe(self->buffer, '"');
        append_string_unsafe(self->buffer, (char *)data, slen);
        append_char_unsafe(self->buffer, '"');
    }
    else {
        if (index_written != slen - 1) {
            if (append_string(self->buffer, &data[index_written + 1], slen - index_written - 1) == -1) {
                return -1;
            }
        }

        if (append_char(self->buffer, '"') == -1) {
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

    if (append_char(self->buffer, '"') == -1) {
        goto bail;
    }

    if (append_bytes(self->buffer, translated_bytes) == -1) {
        goto bail;
    }

    if (append_char(self->buffer, '"') == -1) {
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
_append_dict(Encoder *self, PyObject *dict)
{
    int retval = -1;

    Buffer *b = self->buffer;

    PyObject *key;
    PyObject *value;

    if (self->dict_preserve_order == -1) {
        PyObject *user_dict_preserve_order = PyObject_GetAttrString((PyObject*)self, "DICT_PRESERVE_ORDER");
        if (user_dict_preserve_order == NULL)
            goto bail;

        self->dict_preserve_order = PyObject_IsTrue(user_dict_preserve_order);

        Py_DECREF(user_dict_preserve_order);

        if (self->dict_preserve_order == -1)
            goto bail;
    }

    if (!PyDict_CheckExact(dict) && self->dict_preserve_order == 1) {
        return _append_mapping(self, dict);
    }

    Py_ssize_t pos = 0; /* NOT incremental */
    int index = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        if (append_char(b, (index == 0 ? '{' : ',')) == -1)
            goto bail;

        if (_append(self, key) == -1)
            goto bail;

        if (append_char(b, ':') == -1)
            goto bail;

        if (_append(self, value) == -1)
            goto bail;

        index++;
    }

    if (index == 0) {
        /* Empty. */
        if (append_string(b, "{}", 2) == -1)
            goto bail;
    } else {
        if (append_char(b, '}') == -1)
            goto bail;
    }

    retval = 0;
  bail:
    return retval;
}

Py_LOCAL_INLINE(int)
_append_mapping(Encoder *self, PyObject *mapping) {
    int retval = -1;
    Buffer *b = self->buffer;
    PyObject *items = PyMapping_Items(mapping);

    if (items == NULL)
        goto bail;

    int length = PySequence_Fast_GET_SIZE(items);

    if (length == 0) {
        if (append_string(b, "{}", 2) == -1)
            goto bail;
    }
    else {
        /* Borrowed references */
        PyObject *item;

        int i;

        if (append_char(b, '{') == -1)
            goto bail;

        for (i = 0; i < length; i++) {
            if (i != 0)
                if (append_char(b, ',') == -1)
                    goto bail;

            item = PyList_GET_ITEM(items, i);

            if (_append(self, PyTuple_GET_ITEM(item, 0)) == -1)
                goto bail;

            if (append_char(b, ':') == -1)
                goto bail;

            if (_append(self, PyTuple_GET_ITEM(item, 1)) == -1)
                goto bail;

        }

        if (append_char(b, '}') == -1)
            goto bail;
    }

    retval = 0;
  bail:
    Py_XDECREF(items);
    return retval;
}

Py_LOCAL_INLINE(int)
_append_fast_sequence(Encoder *self, PyObject *sequence)
{
    Buffer *b = self->buffer;

    int retval = -1;
    /* XXX: must be list/tuple, using the assumption macros */
    int length = PySequence_Fast_GET_SIZE(sequence);

    if (length == 0) {
        if (append_string(b, "[]", 2) == -1)
            goto bail;
    }
    else {
        if (append_char(b, '[') == -1)
            goto bail;

        PyObject **items = PySequence_Fast_ITEMS(sequence);
        int i;

        for (i = 0; i < length; i++) {
            if (i != 0)
                if (append_char(b, ',') == -1)
                    goto bail;

            if (_append(self, items[i]) == -1)
                goto bail;
        }

        if (append_char(b, ']') == -1)
            goto bail;
    }

    retval = 0;
  bail:
    return retval;
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

Py_LOCAL_INLINE(int)
_append_bytes_constant(Encoder *self, PyObject **member, const char *name)
{
    PyObject *bytes = *member;

    if (bytes == NULL) {
        PyObject *str = PyObject_GetAttrString((PyObject*)self, name);

        if (str == NULL) {
            return -1;
        }


        bytes = PyUnicode_AsEncodedString(str, NULL, NULL);

        Py_DECREF(str);

        if (bytes == NULL) {
            return -1;
        }

        *member = bytes;
    }

    return append_bytes(self->buffer, bytes);
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

static PyMethodDef methods[] = {
    {"encode",         (PyCFunction)encode,         METH_O, encode___doc__},
    {"encode_bytes",   (PyCFunction)encode_bytes,   METH_O, encode_bytes___doc__},
    {NULL} /* Sentinel */
};

PyTypeObject Encoder_Type = {
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
    __doc__,                   /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    methods,                   /* tp_methods */
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
