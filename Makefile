 TARGET= smug
 TYPE= ps-exe

 SRCS = main.c \
 ../common/crt0/crt0.s \
 TIM/smug64.tim \
VAG/hello.vag \
VAG/poly.vag \

CPPFLAGS 	+= -I../psyq/include
LDFLAGS 	+= -L../psyq/lib
LDFLAGS 	+= -Wl,--start-group
LDFLAGS 	+= -lapi
LDFLAGS 	+= -lc
LDFLAGS 	+= -lc2
LDFLAGS 	+= -lcard
LDFLAGS 	+= -lcomb
LDFLAGS 	+= -lds
LDFLAGS 	+= -letc
LDFLAGS 	+= -lgpu
LDFLAGS 	+= -lgs
LDFLAGS 	+= -lgte
LDFLAGS 	+= -lgun
LDFLAGS 	+= -lhmd
LDFLAGS 	+= -lmath
LDFLAGS 	+= -lmcrd
LDFLAGS 	+= -lmcx
LDFLAGS 	+= -lpad
LDFLAGS 	+= -lpress
LDFLAGS 	+= -lsio
LDFLAGS 	+= -lsnd
LDFLAGS 	+= -lspu
LDFLAGS 	+= -ltap
LDFLAGS 	+= -Wl,--end-group

include ../common.mk \
