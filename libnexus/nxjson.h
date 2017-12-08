/*
 * Copyright (c) 2013 Yaroslav Stavnichiy <yarosla@gmail.com>
 *
 * This file is part of NXJSON.
 *
 * NXJSON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * NXJSON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with NXJSON. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NXJSON_H
#define NXJSON_H

#ifdef  __cplusplus
extern "C" {
#endif


typedef enum nx_json_type {
  NX_JSON_NULL,    // this is null value
  NX_JSON_OBJECT,  // this is an object; properties can be found in child nodes
  NX_JSON_ARRAY,   // this is an array; items can be found in child nodes
  NX_JSON_STRING,  // this is a string; value can be found in text_value field
  NX_JSON_INTEGER, // this is an integer; value can be found in int_value field
  NX_JSON_DOUBLE,  // this is a double; value can be found in dbl_value field
  NX_JSON_BOOL     // this is a boolean; value can be found in int_value field
} nx_json_type;

struct nx_json {
    nx_json_type type;           // type of json node, see above

    char * key;            // key of the property; for object's children only
    
    char * text_value;     // text value of STRING node
    long long    int_value;      // the value of INTEGER or BOOL node
    double       dbl_value;      // the value of DOUBLE node

    int length;                  // number of children of OBJECT or ARRAY
    struct nx_json * child;      // points to first child
    struct nx_json * next;       // points to next child
    struct nx_json * last_child; // points to last child


    char * raw_string;           // Raw JSON string that is stored by the root
};


static struct nx_json * nx_json_parse(char * text);

static void nx_json_free(struct nx_json * js);

static struct nx_json * nx_json_get(struct nx_json * json, char * key); // get object's property by key

#if 0
// Disabled for now so we don't have compile errors
static struct nx_json * nx_json_item(struct nx_json * json, int idx); // get array element by index
#endif

#ifdef  __cplusplus
}
#endif

#endif  /* NXJSON_H */
