# use "m" for development: alias m=tools/m.sh

all:
	# T.B.D.

clean:
	find . -name .pio -print0 | xargs -0 rm -rf
	for i in o mpy out; \
	    do find . -name "*.$$i" -print0 | xargs -0 rm -f; done

.PHONY: all clean
