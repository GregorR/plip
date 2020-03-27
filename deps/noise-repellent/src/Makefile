include ../../../Makefile.share

OBJS= \
	estimate_noise_spectrum.o \
	nrepel.o

all: libnr.a

test: $(OBJS)
	$(CC) $(CFLAGS) test.c $(OBJS) \
		../../fftw/.libs/libfftw3f.a \
		-lm -o $@

libnr.a: $(OBJS)
	$(CROSS_PREFIX)ar -rc $@ $(OBJS)
	$(CROSS_PREFIX)ranlib $@

%.o: %.c
	$(CC) $(CFLAGS) -I ../../fftw/api -Werror -c $< -o $@

clean:
	rm -f test libnr.a $(OBJS)
