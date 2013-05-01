#ifndef _ENCODER_H
#define _ENCODER_H

#include "buffer.h"

extern PyTypeObject Encoder_Type;

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

#endif
