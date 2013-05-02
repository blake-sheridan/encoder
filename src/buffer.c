#include <Python.h>
#include "buffer.h"

#define BUFFER_SIZE_INITIAL 1024
#define BUFFER_SIZE_INCREMENT 1024
#define BUFFER_SIZE_MAX 10240

Buffer* new_buffer()
{
    Buffer *buffer = NULL;
    char *data = PyMem_Malloc(BUFFER_SIZE_INITIAL);

    if (data != NULL) {
        buffer = PyMem_Malloc(sizeof(Buffer));
        if (buffer == NULL) {
            /* XXX: free data? */
        }
        else {
            buffer->_data = data;
            buffer->_index = 0;
            buffer->_size = BUFFER_SIZE_INITIAL;
        }
    }

    return buffer;
}

void delete_buffer(Buffer *buffer)
{
    PyMem_Free(buffer->_data);
}

int
_Buffer_resize(Buffer *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "resizing internal buffer");
    return -1;
}
