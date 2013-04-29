#ifndef _ENCODER_BUFFER_H
#define _ENCODER_BUFFER_H

typedef struct {
    char *_data;
    int _index;
    int _size;
} Buffer;

/* Prototypes */
static Buffer* new_buffer(void);
static void delete_buffer(Buffer *buffer);

Py_LOCAL_INLINE(int) ensure_room     (Buffer *self, int length);

Py_LOCAL_INLINE(int) append_bytes    (Buffer *self, PyObject *bytes);
Py_LOCAL_INLINE(int) append_char     (Buffer *self, const char c);
Py_LOCAL_INLINE(int) append_double   (Buffer *self, double d);
Py_LOCAL_INLINE(int) append_longlong (Buffer *self, long long l);
Py_LOCAL_INLINE(int) append_string   (Buffer *self, char *string, int length);

Py_LOCAL_INLINE(void) append_char_unsafe   (Buffer *self, const char c);
Py_LOCAL_INLINE(void) append_string_unsafe (Buffer *self, char *string, int length);

static PyObject* Buffer_as_bytes (Buffer *self);

/***************************
 * Internal Implementation *
 ***************************/

#define _BUFFER_SIZE_INITIAL 1024
#define _BUFFER_SIZE_INCREMENT 1024
#define _BUFFER_SIZE_MAX 10240

/* TODO: calc these instead of fudging. */
#define _SPRINTF_MAX_LONG_LONG_LENGTH 31
#define _SPRINTF_MAX_DOUBLE_LENGTH 51

/* Prototypes */
static int _Buffer_resize(Buffer *self);

static Buffer* new_buffer()
{
    Buffer *buffer = NULL;
    char *data = PyMem_Malloc(_BUFFER_SIZE_INITIAL);

    if (data != NULL) {
        buffer = PyMem_Malloc(sizeof(Buffer));
        if (buffer == NULL) {
            /* XXX: free data? */
        }
        else {
            buffer->_data = data;
            buffer->_index = 0;
            buffer->_size = _BUFFER_SIZE_INITIAL;
        }
    }

    return buffer;
}

static void delete_buffer(Buffer *buffer)
{
    PyMem_Free(buffer->_data);
}

Py_LOCAL_INLINE(int)
ensure_room(Buffer *self, int length)
{
    if (self->_index + length >= self->_size) {
        if (_Buffer_resize(self) == -1) {
            return -1;
        }
    }
    return 0;
}

Py_LOCAL_INLINE(int)
append_bytes(Buffer *self, PyObject *bytes)
{
    int length = PyBytes_GET_SIZE(bytes);

    if (ensure_room(self, length) == -1) {
        return -1;
    }

    append_string_unsafe(self, PyBytes_AS_STRING(bytes), length);

    return 0;
}

Py_LOCAL_INLINE(int)
append_char(Buffer *self, const char c)
{
    if (self->_index == self->_size)
        if (_Buffer_resize(self) == -1)
            return -1;

    append_char_unsafe(self, c);

    return 0;
}

Py_LOCAL_INLINE(void)
append_char_unsafe(Buffer *self, const char c)
{
    self->_data[self->_index++] = c;
}

Py_LOCAL_INLINE(int)
append_double(Buffer *self, double d)
{
    if (ensure_room(self, _SPRINTF_MAX_DOUBLE_LENGTH) == -1) {
        return -1;
    }

    int num_chars = sprintf(&self->_data[self->_index], "%f", d);
    if (num_chars < 0) {
        PyErr_SetString(PyExc_RuntimeError, "failure from sprintf on float");
        return -1;
    }

    self->_index += num_chars;

    /* Trim trailing 0's - does not seem to be way to tell sprintf to do so. */
    while (self->_data[self->_index - 1] == '0') {
        self->_index--;
    }

    return 0;
}

Py_LOCAL_INLINE(int)
append_longlong(Buffer *self, long long l)
{
    if (l >= 0 && l <= 9) {
        /* Special case - single chars much faster */
        return append_char(self, (char) l + 48);
    }

    if (ensure_room(self, _SPRINTF_MAX_LONG_LONG_LENGTH) == -1) {
        return -1;
    }

    int num_chars = sprintf(&self->_data[self->_index], "%Ld", l);
    if (num_chars < 0) {
        PyErr_SetString(PyExc_RuntimeError, "failure from sprintf on int");
        return -1;
    }

    self->_index += num_chars;

    return 0;
}

Py_LOCAL_INLINE(int)
append_string(Buffer *self, char *string, int length)
{
    if (ensure_room(self, length) == -1) {
        return -1;
    }

    append_string_unsafe(self, string, length);

    return 0;
}

Py_LOCAL_INLINE(void)
append_string_unsafe(Buffer *self, char *string, int length)
{
    strncpy(&(self->_data)[self->_index], string, length);
    self->_index += length;
}

//Py_LOCAL_INLINE(PyObject*)
static PyObject *
Buffer_as_bytes(Buffer *self) {
    return PyBytes_FromStringAndSize(self->_data, self->_index);
}

static int
_Buffer_resize(Buffer *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "resizing internal buffer");
    return -1;
}

#endif
