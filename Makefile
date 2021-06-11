# use "m" for development: alias m=$PWD/scripts/m.sh

all:
	# T.B.D.

clean:
	find . -name .pio -print0 | xargs -0 rm -r
	for i in o mpy out; do find . -name "*.$$i" -print0 | xargs -0 rm; done

.PHONY: all clean
