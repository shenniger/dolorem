set debuginfod enabled off
set auto-solib-add off
file ./dolorem
start repl.dlr
shared libdolorem.so
break gdb_break_here
break compiler_error
