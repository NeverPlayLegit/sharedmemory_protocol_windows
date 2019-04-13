# Small Sharedmemory-Protocol for Windows
Small protocol for simple sharedmemory communication between two programs under windows.

### Example

```C
#include "sh_client.h"

void callback(sh_client_t* sh, int8_t *buf, uint32_t len) {
	printf("Received: %.*s\n", len, buf);
}

int main() {
	sh_client_t* sh = sh_client_new((_sh_client_recv_callback)&callback
		, "Global\\SharedTest1", "Global\\SharedTest2"
		, "Global\\SharedTest2E", "Global\\SharedTest1E"
		, "Global\\SharedTest2E_", "Global\\SharedTest1E_"
		, 256);
	if (sh == 0) printf("Error%d\n", GetLastError());

	int8_t* buf;
	buf = (int8_t*)malloc(200);
	memset(buf, 0, 200);
	while (1) {
		scanf("Type what to send: %s", buf);
		sh_client_send(sh, buf, strlen(buf));
		memset(buf, 0, 200);
	}

	sh_client_close(sh);
	return 0;
}
```

For the other partner just swap the names: "Global\\SharedTest2" and "Global\\SharedTest1"

```C
sh_client_t* sh_other = sh_client_new((_sh_client_recv_callback)&callback, "Global\\SharedTest2", "Global\\SharedTest1", 256);
```
