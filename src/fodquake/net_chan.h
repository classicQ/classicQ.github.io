#ifndef __NET_CHAN_H__
#define __NET_CHAN_H__

#include "net.h"

#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct
{
	qboolean	fatal_error;

	enum netsrc	sock;

	int			dropped;			// between last packet and previous

	float		last_received;		// for timeouts

	// the statistics are cleared at each client begin, because
	// the server connecting process gives a bogus picture of the data
	float		frame_latency;		// rolling average
	float		frame_rate;

	int			drop_count;			// dropped packets, cleared each level
	int			good_count;			// cleared each level

	struct netaddr	remote_address;
	int			qport;

	// bandwidth estimator
	double		cleartime;			// if curtime > nc->cleartime, free to go
	double		rate;				// seconds / byte

	// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

	// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN];

	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN];	// unacked reliable message

	// time and size data to calculate bandwidth
	int			outgoing_size[MAX_LATENT];
	double		outgoing_time[MAX_LATENT];

	/* Anything below here is not cleared by Netchan_Setup */
	struct HuffContext *huffcontext;
} netchan_t;

void Netchan_CvarInit(void);
void Netchan_Transmit (netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand(enum netsrc sock, struct netaddr adr, int length, byte *data);
void Netchan_OutOfBandPrint(enum netsrc sock, struct netaddr adr, char *format, ...);
qboolean Netchan_Process(netchan_t *chan, sizebuf_t *message);
void Netchan_Setup (enum netsrc sock, netchan_t *chan, struct netaddr adr, int qport);

qboolean Netchan_CanPacket (netchan_t *chan);
qboolean Netchan_CanReliable (netchan_t *chan);

#endif

