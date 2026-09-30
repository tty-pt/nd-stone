all := libnd-stone

LDLIBS-libnd-stone := -lxylem


# macOS ld rejects undefined symbols in shared libs, but WARN needs
# qsyslog: an engine-provided function pointer resolved at dlopen (Linux
# allows this by default). dynamic_lookup is the Darwin equivalent.
LDFLAGS-libnd-stone-Darwin += -undefined dynamic_lookup
-include ./../mk/include.mk
