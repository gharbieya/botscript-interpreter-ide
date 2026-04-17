#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* compile_json(const char* source) {
  (void)source;
  const char* json = "{\"ok\":true,\"errors\":[],\"tokens\":[],\"ast\":null}";
  char* out = (char*)malloc(strlen(json) + 1);
  if (!out) return NULL;
  strcpy(out, json);
  return out;
}

#ifdef __cplusplus
}
#endif
