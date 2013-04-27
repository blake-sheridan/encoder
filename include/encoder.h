#ifndef _ENCODER_H
#define _ENCODER_H

#define BUFFER_INITIAL_LENGTH 1024
#define BUFFER_INCREMENT 1024
#define BUFFER_MAX_LENGTH 10240

/* TODO: calc these instead of fudging. */
#define SPRINTF_MAX_LONG_LONG_LENGTH 31
#define SPRINTF_MAX_DOUBLE_LENGTH 51

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

static PyObject* encode                     (Encoder *self, PyObject *o);
static PyObject* encode_bytes               (Encoder *self, PyObject *o);

static int           _append                (Encoder *self, PyObject *o);
Py_LOCAL_INLINE(int) _append_int            (Encoder *self, PyObject *py_int);
Py_LOCAL_INLINE(int) _append_float          (Encoder *self, PyObject *py_float);

Py_LOCAL_INLINE(int) _append_str            (Encoder *self, PyObject *str);
Py_LOCAL_INLINE(int) _append_str_1byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_2byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_4byte_kind (Encoder *self, PyObject *str, int length);
Py_LOCAL_INLINE(int) _append_str_naive      (Encoder *self, PyObject *str, int length);

Py_LOCAL_INLINE(int) _append_bytes          (Encoder *self, PyObject *bytes);
Py_LOCAL_INLINE(int) _append_dict           (Encoder *self, PyObject *dict);
Py_LOCAL_INLINE(int) _append_fast_sequence  (Encoder *self, PyObject *list_or_tuple);
Py_LOCAL_INLINE(int) _append_mapping        (Encoder *self, PyObject *mapping);

/*
 * Lazy initialization accessors.
 * Should be the only means of getting their respective attributes.
 */
Py_LOCAL_INLINE(PyObject*)  _get_str_translation_table (Encoder *self);
Py_LOCAL_INLINE(Py_UCS1 **) _get_str_ucs1_mapping      (Encoder *self);

static int _resize(Encoder *self);

Py_LOCAL_INLINE(int) _write_bytes(Encoder *self, PyObject *bytes);
Py_LOCAL_INLINE(int) _write_bytes_constant(Encoder *self, PyObject **member, const char *attrname);

static void _xfree_str_ucs1_mapping(Encoder *self);

#endif
