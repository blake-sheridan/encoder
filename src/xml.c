#include <Python.h>
#include "buffer.h"
#include "encoder.h"
#include "xml.h"

/* Forward declarations */
static PyTypeObject Element_Type;

PyDoc_STRVAR(Tag__doc__,
"TODO Tag __doc__");

static PyObject *
Tag__new__(PyTypeObject *type, PyObject *args, PyObject **kwargs)
{
    Tag *self = (Tag *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O!s", &Encoder_Type, &self->encoder, &self->name)) {
        return NULL;
    }

    self->name_length = strlen(self->name);

    return (PyObject *)self;
}

static void
Tag__del__(Tag* self)
{
    //Py_XDECREF(self->encoder);
    /* Free self->name ? */
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Tag__call__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    if (PyTuple_GET_SIZE(args) != 0) {
        PyErr_Format(PyExc_TypeError, "Tag.__call__: does not take positional arguments: %R", args);
        return NULL;
    }

    /* Maybe not do this lazily? */
    if (PyType_Ready(&Element_Type) == -1) {
        return NULL;
    }

    /* TODO: not sure if this is idiomatic. */
    Element *element = PyType_GenericNew(&Element_Type, NULL, NULL);
    if (element == NULL) {
        return NULL;
    }

    element->tag = self;
    element->attributes = kwargs;

    return element;
}

PyTypeObject Tag_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_encoder.Tag",            /* tp_name */
    sizeof(Tag),               /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Tag__del__,    /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    Tag__call__,               /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    Tag__doc__,                /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    Tag__new__,                /* tp_new */
};

PyDoc_STRVAR(Element__doc__,
"TODO Element __doc__");

static void
Element__del__(Element* self)
{
    //Py_XDECREF(self->tag);
    //Py_XDECREF(self->attributes);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Element__enter__(Element *self, PyObject *args)
{
    if (self->attributes == NULL) {
        if (ensure_room(self->tag->encoder->buffer, self->tag->name_length + 2) == -1)
            return NULL;

        append_char_unsafe(self->tag->encoder->buffer, '<');
        append_string_unsafe(self->tag->encoder->buffer, self->tag->name, self->tag->name_length);
        append_char_unsafe(self->tag->encoder->buffer, '>');
    }
    else {
        PyErr_Format(PyExc_NotImplementedError, "Element.__enter__ with attributes %R", self->attributes);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
Element__exit__(Element *self, PyObject *args)
{
    if (ensure_room(self->tag->encoder->buffer, self->tag->name_length + 3) == -1)
        return NULL;

    append_char_unsafe(self->tag->encoder->buffer, '<');
    append_char_unsafe(self->tag->encoder->buffer, '/');
    append_string_unsafe(self->tag->encoder->buffer, self->tag->name, self->tag->name_length);
    append_char_unsafe(self->tag->encoder->buffer, '>');

    Py_RETURN_NONE;
}

static PyMethodDef Element_methods[] = {
    {"__enter__", Element__enter__, METH_NOARGS, NULL},
    {"__exit__",  Element__exit__,  METH_VARARGS, NULL},
    {NULL} /* Sentinel */
};

static PyTypeObject Element_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_encoder.Element",        /* tp_name */
    sizeof(Element),           /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Element__del__,/* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    Element__doc__,            /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Element_methods,           /* tp_methods */
};
