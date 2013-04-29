#ifndef _ENCODER_H
#define _ENCODER_H

#include "buffer.h"

typedef struct {
    PyObject_HEAD

    Buffer *buffer;

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

static void _xfree_str_ucs1_mapping(Encoder *self);

#endif
