set auto-solib-add off
file ./dolorem
start a.dlr
shared libdolorem.so
break gdb_break_here
break compiler_error
