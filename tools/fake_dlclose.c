#include <stdio.h>
int dlclose(void *handle) {
	fprintf(stderr, "[FAKE DLCLOSE] : preventing dlclose for %p\n", handle);
}
