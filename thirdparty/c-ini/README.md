# No Bullshit INI parser for C

## Basic usage

If  you  want to serialize/deserialze a struct, simply prefix that struct  with
```SECTION("section name")```:

```c
#include "player_data_ini_parser.h"

SECTION("player")
struct player_data
{
    char* name;
    int health;
    float x, y, z;
};

static const char* config =
    "[player]\n"
    "name = \"Bob\"\n"
    "health = 80\n";

int main(void)
{
    struct player_data player;
    player_data_init(&player);  /* Sets default values */
    player_data_parse(&player, "<stdin>", config, strlen(config));
    player_data_fwrite(&player, stdout);
    player_data_deinit(&player);

    return 0;
}
```

This    makes    the     struct     visible     to    the    code    generator.
```player_data_ini_parser.h``` is the generated header file. This declares some
functions  used  to  load  and  save  the  struct  to  and  from  an INI  file.

## Building

### With CMake

If  you  are  using  a  CMake  project,  simply  ```add_subdirectory()```  this
repository. This will give you a new CMake function:

```cmake
c_ini_generate (my_parser
    INPUT
        "struct1.h"
        "struct2.h"
        "struct3.h"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/my_parser.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/my_parser.c")

add_executable (my_application "main.c")
# Appends the generated C source file to the executable target
target_link_liraries (my_application PRIVATE my_parser)
# This is so we can #include "my_parser.h"
target_include_directories (my_application PRIVATE "${PROJECT_BINARY_DIR}")
```

You can specify as many input files  as  you want. They can be header or source
files. The  code  generator  will scan each input file and create functions for
every struct it finds.

### Use directly

If you are not using CMake, no worries. The code generator consists of a single
C source file, which you can compile with:

```sh
gcc -o c_ini_generator c-ini/c-ini.c
```

Now you can generate  header/source file pairs from a list of input files with:

```sh
./c_ini_generator \
    --input struct1.h struct2.h struct3.h \
    --output-header parser.h \
    --output-source parser.c
```

Finally, you can compile ```parser.h``` and  ```parser.c``` files and link them
with your application. Note  that  you  will need to add an include path to the
file  ```c-ini.h```  so  that  macros  such  as  ```SECTION()```  are  defined:

```sh
gcc -I./c-ini/ parser.c -c -o parser.o
gcc -I./c-ini/ main.c -c -o main.o
gcc -o application parser.o main.o
```

## Advanced Features

### Default values and constraints

You can optionally add default values and constraints to each member:

```c
SECTION("player")
struct player_data
{
    char* name DEFAULT("Player");
    int health DEFAULT(100) CONSTRAIN(0, 100);
    float x, y, z;
};
```

```DEFAULT()``` modifies the ```_init()``` function to use the custom default values.

```CONSTRAIN()``` adds  checks  to the ```_parse()``` function. If the INI file
contains  a  value  outside  of  the  constrained  range,  then it will  error.

### Strings

The  generator  comes with a default implementation  for  strings  which  calls
```malloc()``` every time an attribute is set. You might not want  this. If you
want  to use your own string type, you can implement the  following  functions:

```c
struct my_str {
    int len;
    char data[];
};

int custom_str_init(struct my_str** str) {
    *str = NULL;
    return 0;  /* success. -1 for failure */
}

void custom_str_deinit(struct my_str* str) {
    if (str)
        free(str);
}

int custom_str_set(struct my_str** str, const char* data, int len) {
    *str = malloc(len + sizeof(int));
    memcpy(str->data, data, len);
    str->len = len;
    return 0;  /* success. -1 for failure */
}

const char* custom_str_data(const struct my_str* str) {
    return str->data;
}

int custom_str_len(const struct my_str* str) {
    return str->len;
}
```

Now you can use the ```STRING()``` attribute to tell the generator to call your
functions instead. The argument is the prefix of the functions to call:

```c
SECTION("player")
struct player_data
{
    char* name STRING(custom_str);
    int health;
    float x, y, z;
};
```

