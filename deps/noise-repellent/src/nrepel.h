#ifndef NREPEL_H
#define NREPEL_H 1

/**
* Enumeration of LV2 ports.
*/
typedef enum {
	NREPEL_AMOUNT = 0,
	NREPEL_NOFFSET = 1,
	NREPEL_RELEASE = 2,
	NREPEL_MASKING = 3,
	NREPEL_T_PROTECT = 4,
	NREPEL_WHITENING = 5,
	NREPEL_N_LEARN = 6,
	NREPEL_N_ADAPTIVE = 7,
	NREPEL_RESET = 8,
	NREPEL_RESIDUAL_LISTEN = 9,
	NREPEL_ENABLE = 10,
	NREPEL_LATENCY = 11,
	NREPEL_INPUT = 12,
	NREPEL_OUTPUT = 13,
} PortIndex;

void *
nrepel_instantiate(double rate);

void
nrepel_connect_port(void *instance, uint32_t port, void *data);

void
nrepel_run(void *instance, uint32_t n_samples);

void
nrepel_cleanup(void *instance);

#endif
