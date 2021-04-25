all:
	ccpsx -O3 -Xo$80010000 smug64.obj main.c -omain.cpe,main.sym,mem.map
	ccpsx -O3 -Xo$80010000 smug64.obj main.c \psx\lib\none2.obj -omain.cpe

	cpe2x /ce main.cpe
#this is the old makefile for when I was using psymake.
#the new Makefile uses gcc with the pcsx-redux method
