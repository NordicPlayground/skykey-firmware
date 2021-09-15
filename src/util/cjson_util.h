
#include <cJSON.h>

/**
 * @brief Gets a pointer to the property "string" if it exists and creates it if it doesn't.
 * Case insensitive.
 * 
 * @param object Object to get the property from.
 * @param string Name of the property.
 * @return Pointer to the property. NULL if the property does not exist and could not be created.
 */
static inline cJSON *cJSON_GetOrAddObjectItem(cJSON *object, const char *string){
    cJSON *prop = cJSON_GetObjectItem(object, string);
    if (prop == NULL){
        prop = cJSON_AddObjectToObject(object, string);
    }
    return prop;
}

/**
 * @brief Gets a pointer to the property "string" if it exists and creates it if it doesn't.
 * Case sensitive.
 * 
 * @param object Object to get the property from.
 * @param string Name of the property.
 * @return Pointer to the property. NULL if the property does not exist and could not be created.
 */
static inline cJSON *cJSON_GetOrAddObjectItemCS(cJSON *object, const char *string){
    cJSON *prop = cJSON_GetObjectItemCaseSensitive(object, string);
    if (prop == NULL){
        prop = cJSON_AddObjectToObjectCS(object, string);
    }
    return prop;
}