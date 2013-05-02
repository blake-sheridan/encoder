#ifndef _ENCODER_XML_H
#define _ENCODER_XML_H

typedef struct {
    PyObject_HEAD
    Encoder *encoder;
    char *name;
    int name_length;
} Tag;

typedef struct {
    PyObject_HEAD
    Tag *tag;
    PyObject *attributes;
} Element;

/* For module.c */
PyAPI_DATA(PyTypeObject) Tag_Type;

#endif
