#!/usr/bin/python2.5
#
# Copyright 2016 Emilie Gillet.
#
# Master resources file.

header = """// Copyright 2016 Emilie Gillet.
//
// Resources definitions.
//
// Automatically generated with:
// make resources
"""

namespace = 'plaits'
target = 'plaits'
types = ['uint8_t', 'uint16_t']
includes = """
#include "stmlib/stmlib.h"
"""

import lookup_tables

create_specialized_manager = True

resources = [
  (lookup_tables.lookup_tables,
   'lookup_table', 'LUT', 'float', float, False),
  (lookup_tables.lookup_tables_i16,
   'lookup_table_i16', 'LUT', 'int16_t', int, False),
  (lookup_tables.lookup_tables_i32,
   'lookup_table_i32', 'LUT', 'int32_t', int, False),
  (lookup_tables.lookup_tables_i8,
   'lookup_table_i8', 'LUT', 'int8_t', int, False)
]
