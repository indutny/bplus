# BPlus - CoW B+ tree in C [![Build Status](https://secure.travis-ci.org/indutny/bplus.png)](http://travis-ci.org/indutny/bplus)


## Compiling

```bash
git clone git://github.com/indutny/bplus.git
cd bplus
make MODE=release
ls bplus.a # <- link your project with that file
```

## Usage

```C
#include <stdlib.h>
#include <stdio.h>

#include "bplus.h"

int main(void) {
  bp_db_t db;

  /* Open database */
  bp_open(&db, "/tmp/1.bp");

  /* Set some value */
  bp_sets(&db, "key", "value");

  /* Get some value */
  bp_value_t value;
  bp_gets(&db, "key", &value);
  fprintf(stdout, "%s\n", value.value);
  free(value.value)

  /* Close database */
  bp_close(&db);
}
```

See [include/bplus.h](https://github.com/indutny/bplus/blob/master/include/bplus.h) for more details.

## Advanced build options

```bash
make MODE=debug # build with enabled assertions
make SNAPPY=0 # build without snappy (no compression will be used)
```

#### LICENSE

This software is licensed under the MIT License.

Copyright Fedor Indutny, 2012.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.
