#ifndef EPECUR_H
#define EPECUR_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Our Ports	*/
#define PORTC 0x5629	// control port - bidirectonal, TCP
#define PORTD 0x5498	// data port - data to collector, TCP
#define PORTS 5630	// slow data port - slow data to collector, UDP
#define PORTE 0x5301	// Event publisher port, TCP
#define PORTL 0x5702	// logger port - messages to logger, UDP
#define CSERVER "main.epecur.local"	// the command server
#define DSERVER "main.epecur.local"	// the data server
#define ESERVER "main.epecur.local"	// the event server
#define LSERVER "main.epecur.local"	// the logging server

#define MEMCHUNK  0x10000	// 64 k

int ConnectSocket(char *name, int port);

/*	Logger 		*/
#define MAXMSGLEN 1000
typedef struct {
    int ID;			// message source ID
    int type;			// message type
    int flags;			// message flags
    char msg[MAXMSGLEN];	// message text
} LogMessageStruct;

#define MSG_DEBUG  0
#define MSG_INFO   1
#define MSG_WARN   2
#define MSG_ERROR  3
#define MSG_FATAL  4

void LogPrint(int type, int flags, const char *fmt, ...);
void LogInit(char *hostname, int ID);

/*	Configuration	*/
typedef struct {
    int serial;		// board number and type; 0 - an empty slot
    int logical;	// board logical (position) number
    char fwname[512]; 	// path to the firmware file
    char xilname[512];	// path to the xilinx bitstream
    int datatype;	// type of data produced: 0 - proportional, 1 - drift, 2 - triglog, 3 - trigbox, 4 - CAMAC, ...
    int trigmask;	// mask of triggers sent to this board
} ConfigurationStruct;

#define MAXBOARDS	300	// maximum number of DAQ boards
#define MAXUSB		50	// maximum number of DAQ boards per switch box
#define MAXPROC		50	// maximum number of online processors
#define MAXPKTCOUNTERS	1000	// maximum number of packet types for statistics

#define MAXEQUEUE 10	// queue for event requests
#define EXP_CYCLE 1	// request to send cycle buffer

void InitConfiguration(void);
int ReadConfiguration(char *fname);
ConfigurationStruct *FindConfEntry(int serial);

/*	EPdaemon		*/	
#define EPECURID     0x2956	// our Vendor ID
#define BCD_FWLOADED 0x1000	// firmware is loaded bit in bcdDevice
#define BCD_SERIAL   0x0FFF	// serial number in bcdDevice
#define USB_SLEEP    3		// time between USB bus scans 
#define XILRETRIES   5		// retries to load Xilinx
#define USBWAIT      10000	// 10 s.
#define DBUFSIZE     0x200000	// 2 MByte
#define DCHUNK       0x4000     // 16 kByte
#define SLOWBUFSIZE  0x10000	// 64 kByte

/*	Commands 		*/
				//					Command			Response
typedef enum {			//	Description		parameters	data	parameters	data
    CMD_NONE, 			// dummy			-		-	-		-
    CMD_GETSERIAL, 		// get board serial number	-		-	serial	-
    CMD_START, 			// start EP2 streaming		-		-	-		-
    CMD_STOP, 			// stop EP2 streaming		-		-	-		-
    CMD_DAC, 			// set DAC value (threshold)	DAC		-	-		-
    CMD_PROG, 			// Reprog board(CPU and Xilinx)	-		-	-		-
    CMD_STATUS,			// get status (DONE etc.)	-		-	DONE,DAC	-
    CMD_REGREAD,		// read Xilinx register		address		-	value		-
    CMD_REGWRITE,		// write Xilinx register	address,value	-	-		-
    CMD_ROMREAD,		// read EEPROM			-		-	-		16bytes
    CMD_ROMWRITE,		// write EEPROM			-		16bytes	-		-
    CMD_DELAY,			// set trigger Delay		delay		-	-		-
    CMD_DTIM,			// set channel dead time	DTIM		-	-		-
    CMD_SIGLEN,			// set signal length		SIGLEN		-	-		-
    CMD_INTTRIG,		// set internal trigger		0/1 - Ext/Int	-	-		-
    CMD_DATAENABLE,		// Enable data to USB		0/1 - dis/enb	-	-		-
    CMD_TRIGDELAY,		// Dealy for trigger output	delay		-	-		-
    CMD_RSFIFO,			// reset FIFO			-		-	-		-
    CMD_SOFTTRIG,		// pulse software trigger	-		-	-		-
    CMD_TRIGMASK,		// mask for channels in trig.	-		mask	-		-
    CMD_MASK,			// mask channels		-		mask	-		-
    CMD_GETLEVELS,		// values at the input		-		-	-		mask
    CMD_PKTEND			// send PKT_END			-		-	-		-
} CMDTYPE;

#define MAXCMDLEN 4096
typedef struct {
    int cmd;			// the command itself
    int param[4];		// place for upto 4 parameters
    char data[MAXCMDLEN];	// data of MAXCMDLEN maximum
} CommandStruct;

typedef struct {
    int len;			// total length returned in bytes, <=0 on error.
    int param[4];		// place for upto 4 parameters
    char data[MAXCMDLEN];	// data of MAXCMDLEN maximum
} ResponseStruct;

typedef struct {
    int socket;			// socket for this connection
    int serial;			// board serial number
    int logical;		// board logical number
    int datatype;		// board datatype
    int trigmask;		// trigger mask
    int host;			// remote host IP
    int select;			// selection flag
} ConnectionStruct;


#define DTYPE_PROP	0
#define DTYPE_DRIFT	1
#define DTYPE_HODO	2
#define DTYPE_TRIG	3
#define DTYPE_CAMAC	4

int CreateListenSocket(int port);

/*	Markers in the data stream	*/
typedef enum {MARKER_EOC, MARKER_CLOSE} MARKERS;
int SendMarker(int chan, MARKERS marker);
unsigned char *SearchMarker(const unsigned char *buf, const int len);
#define MARKER_SIZE 8
#define MARKER_BUF char MarkerBuf[MARKER_SIZE] = {0xFF, 0xC0, 0xCA, 0xD5, 0xAA, 0xD5, 0xAA, 0x80}

/*	File structure		*/
typedef struct {
    int len;			// record full length in bytes
    int type;			// record type
    int lintime;		// linux time
    int number;			// cycle number (or something similar)
    int flag;			// flags
} RecordHeaderStruct;

void PktEnd(int all);

#define REC_CYCLE 0x100		// events in one cycle (type of the record, not a number)
#define REC_SLOW  0x1000	// slow data received
#define REC_RAW	  0x10000	// Raw data record

typedef struct {
    int len;			// record full length in bytes
    int type;			// record type
    int lintime;		// linux time
    int number;			// program cycle number when the record was received
    int flag;			// flags
    int logical;		// logical board number
    int serial;			// serial board number
    int datatype;		// board datatype
    int trigmask;		// trigger mask
    unsigned char data[0];	// place for data
} RawRecordStruct;

typedef struct {
    int len;			// event full length
    int number;			// trigger number
    int flag;			// flags
} EventHeaderStruct;

void dsleep(double val);

// Slow record types and structures

// LiH Target
#define EP_SRCID_TGT 	1001
typedef struct {
    double R[8];	// Resistors in the target
    double V[7];	// Pressues and vacuum
    double H[8];	// Resistors in the He
    int Sel;		// Target resistor manually selected
    time_t t;		// Time of measurement
} lcData;
typedef struct {
    int length;
    int type;
    lcData tgt;
} Slow_Tgt;

// NMR
#define EP_SRCID_NMR 	1002
typedef struct { short sig; short sw; } buft;
typedef struct {
    int length;
    int type;
    int good;
    float freq;
    float freq0;
    float balance;
    float sweepampl;
    float corrcoef;
    float fieldcoef;
    int npoints;
    buft scope[550];
} Slow_NMR;

// Trigbox -- actually register content
#define EP_SRCID_TRGB 	1003
typedef struct {
    int length;
    int type;
    unsigned int regs[46];
} Slow_TBox;

// Camera
#define EP_SRCID_CAMERA	1004
typedef struct {
    int length;
    int type;
    unsigned char jpeg[0];
} Slow_Camera;

#ifdef __cplusplus
}
#endif

#endif /* EPECUR_H */
