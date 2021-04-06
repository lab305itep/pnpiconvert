#ifndef RECFORMAT_H
#define RECFORMAT_H

struct rec_header_struct {
	int len;
	int cnt;
	int ip;
	int type;
	int time;
};

struct hw_rec_struct {
	unsigned int len:9;	// 9 bit length of record in short int without the first word
	unsigned int chan:6;	// channel number
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type
	unsigned int sig0:1;	// must be 0
	short int d[0];		// data goes here
};

struct hw_rec_struct_self {
	unsigned int len:9;	// 9 bit length of record in short int without the first word
	unsigned int chan:6;	// channel number
	unsigned int sig1:1;	// must be 1
	unsigned int cnt:10;	// 10 bit self trigger countert
	unsigned int sige:1;	// must be 0
	unsigned int parity:1;	// parity, no way to use here
	unsigned int type:3;	// record type (0 for this record)
	unsigned int sig0:1;	// must be 0
	short int pedestal;	// current pedestal value
	short int d[0];		// waveform data goes here
};

struct hw_rec_struct_mast {
	unsigned int len:9;	// 9 bit length of record in short int without the first word
	unsigned int chan:6;	// channel number
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (1 for this record)
	unsigned int sig0:1;	// must be 0
	short int time;		// time correction 0 to 5, 7  means error
	short int d[0];		// waveform data goes here
};

struct hw_rec_struct_trig {
	unsigned int len:9;	// 9 bit length of record in short int without the first word (7 for this record).
	unsigned int src:6;	// trigger source mask
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (2 for this record)
	unsigned int sig0:1;	// must be 0
	short int user;		// user word
	short int gtime[3];	// global time, us
	short int cnt[2];	// trigger counter
};

struct hw_rec_struct_raw {
	unsigned int len:9;	// 9 bit length of record in short int without the first word
	unsigned int chan:6;	// channel number
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (3 for this record)
	unsigned int sig0:1;	// must be 0
	short int time;		// time correction 0 to 5, 7  means error
	short int d[0];		// waveform data goes here
};

struct hw_rec_struct_sum {
	unsigned int len:9;	// 9 bit length of record in short int without the first word
	unsigned int rsrv1:4;	// reserved
	unsigned int chan:2;	// channel number
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (4 for this record)
	unsigned int sig0:1;	// must be 0
	short int rsrv2;	// reserved
	short int d[0];		// waveform data goes here
};

struct hw_rec_struct_delim {
	unsigned int len:9;	// 9 bit length of record in short int without the first word (4 for this record).
	unsigned int rsrv:6;	// reserved
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (5 for this record)
	unsigned int sig0:1;	// must be 0
	short int gtime[3];	// global time, us
};

struct prop_hit {
	unsigned char wire;	// hit wire number 
	unsigned char brd;	// prop board (must be <128)
};

struct hw_rec_struct_prop {
	unsigned int len:9;	// 9 bit length of record in short int without the first word.
	unsigned int rsrv:6;	// reserved
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// must be 0
	unsigned int type:3;	// record type (6 for this record)
	unsigned int sig0:1;	// must be 0
	struct prop_hit hit[0];	// hits: 
};

struct hw_rec_struct_cycle {
	unsigned int len:9;	// 9 bit length of record in short int without the first word (8 for this record).
	unsigned int src:6;	// trigger source mask
	unsigned int sig1:1;	// must be 1
	unsigned int module:12;	// 12 bit module number (different in self trigger)
	unsigned int type:3;	// record type (7 for this record)
	unsigned int sig0:1;	// must be 0
	short int gtime[3];	// global time, us
	short int cnt[2];	// trigger counter
	short int ccnt[2];	// cycle counter
};

union hw_rec_union {
	short int s[0];
	struct hw_rec_struct       rec;
	struct hw_rec_struct_self  self;
	struct hw_rec_struct_mast  mast;
	struct hw_rec_struct_trig  trig;
	struct hw_rec_struct_raw   raw;
	struct hw_rec_struct_sum   sum;
	struct hw_rec_struct_delim delim;
	struct hw_rec_struct_prop  prop;
	struct hw_rec_struct_cycle  cycle;
};

#pragma pack(1)
struct pre_hit_struct {
	float amp;
	float time;
	short int mod;
	short int chan;
};

struct pre_event_struct {
	int len;
	int systime;
	int number;
	long long gtime;
	int nhits;
	struct pre_hit_struct hit[0];
};
#pragma pack()

//	Record types formed by UFWDTOOL
#define REC_BEGIN	1		// Begin of file / data from the crate
#define REC_PSEOC	10		// Marker for the end of pseudo cycle
#define REC_END		999		// End of file / data from the crate
#define REC_WFDDATA	0x00010000	// Regular wave form data
//	Record types formed by DSINK
#define REC_TYPEMASK	0xFF000000	// Mask for record types
#define REC_SELFTRIG	0x01000000	// SelfTrigger
#define REC_SERIALMASK	0x0000FFFF	// module serial number
#define REC_CHANMASK	0x00FF0000	// channel mask
#define REC_EVENT	0x80000000	// Event
#define REC_EVTCNTMASK	0x7FFFFFFF	// Event counter - the 10 LSB - token

//	Data block type
#define TYPE_SELF	0		// Self trigger waveform 
#define TYPE_MASTER	1		// Master trigger waveform 
#define TYPE_TRIG	2		// Trigger information
#define TYPE_RAW	3		// Raw waveform
#define TYPE_SUM	4		// digital sum of channels waveform 
#define TYPE_DELIM	5		// synchronisation delimiter
#define TYPE_PROP	6		// proportional chambers data
#define TYPE_CYCLE	7		// long trigger (cycle) marker

//	Channel typenum
#define TYPE_SIPM	0
#define TYPE_PMT	1
#define	TYPE_VETO	2


#endif /* RECFORMAT_H */

