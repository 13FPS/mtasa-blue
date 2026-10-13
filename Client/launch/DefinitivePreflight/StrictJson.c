/* MTA preflight needs an unambiguous manifest. Reuse the shipped json-c parser,
 * rejecting duplicate object keys locally instead of changing vendor behavior
 * for existing MTA JSON consumers. The parser already handles this add failure.
 */
#include <json-c/json_object.h>

static int PreflightAddUnique(struct json_object* object, const char* key, struct json_object* value)
{
    struct json_object* previous = NULL;
    if (json_object_object_get_ex(object, key, &previous))
        return -1;
    return json_object_object_add(object, key, value);
}

#define json_object_object_add PreflightAddUnique
#pragma warning(push, 0)
#include "../../../vendor/json-c/json_tokener.c"
#pragma warning(pop)
#undef json_object_object_add
