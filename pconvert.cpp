/*
	Merge two separate data files from dsink and ecollector to 
	a single file. Format is similar to dsink
*/
#define _FILE_OFFSET_BITS 64
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "epecur.h"
#include "recformat-2.h"

/****************************************************************************************************************************************
 *							Class OutEvent									*
 ****************************************************************************************************************************************/

class OutEvent {
private:
	char *buf;
	int size;
	struct rec_header_struct *head;
	void Realloc(int newsize);
public:
	OutEvent(void);
	~OutEvent(void);
	void Add(void *d, int len);
	int Read(FILE *f);
	void Write(FILE *f);
};

OutEvent::OutEvent(void)
{
	buf = NULL;
	size = 0;
	head = NULL;
}

OutEvent::~OutEvent(void)
{
	if (buf) free(buf);
}

void OutEvent::Add(void *d, int len)
{
	if (!head) {
		printf("Internal error - Add before Set\n");
		throw "Internal error - Add before Set";
	}
	if (head->len + len > size) Realloc(head->len + len);
	memcpy(&buf[head->len], d, len);
	head->len += len;
}

int OutEvent::Read(FILE *f)
{
	int irc;
	int len;

	irc = fread(&len, sizeof(int), 1, f);
	if (irc != 1) {
		if (irc != 0) printf("Strange read of VME file %m\n");
		return -1;
	}
	if (len < sizeof(struct rec_header_struct)) {
		printf("Strange file record of %d bytes found\n", len);
		return -2;
	}
	if (len > size) Realloc(len);
	memcpy(buf, &len, sizeof(int));
	irc = fread(&buf[sizeof(int)], len - sizeof(int), 1, f);
	if (irc != 1) {
		printf("Unexpected EOF - %m\n");
		return -3;
	}
	if (head->type & REC_EVENT) return 1;
	return 0;
}

void OutEvent::Realloc(int newsize)
{
	char *tmp;
	
	tmp = (char *)realloc(buf, newsize);
	if (!tmp) {
		printf("No memory: %m\n");
		throw "No memory";
	}
	buf = tmp;
	head = (struct rec_header_struct *) tmp;
	size = newsize;
}

void OutEvent::Write(FILE *f)
{
	int irc;
	irc = fwrite(buf, head->len, 1, f);
	if (irc != 1) {
		printf("Output file write error: %m\n");
		throw "File write error";
	}
}

/****************************************************************************************************************************************
 *							class PropCycle									*
 ****************************************************************************************************************************************/
#define MAXHITS	1000

class PropCycle {
private:
	char *buf;
	int size;
	RecordHeaderStruct *head;
	int rptr;
	struct hw_rec_struct_prop *evtOut;
	void Realloc(int newsize);
	void StoreHit(int num, int wire);
public:
	PropCycle(void);
	~PropCycle(void);
	struct hw_rec_struct_prop *GetNextTrigger();
	int Read(FILE *f);
};

PropCycle::PropCycle(void)
{
	buf = NULL;
	size = 0;
	head = NULL;
	rptr = 0;
	evtOut = (struct hw_rec_struct_prop *)malloc(sizeof(struct hw_rec_struct_prop) + MAXHITS * sizeof(struct prop_hit));
	evtOut->rsrv = 0;
	evtOut->sig1 = 1;
	evtOut->module = 0;
	evtOut->type = 6;
	evtOut->sig0 = 0;
}

PropCycle::~PropCycle(void)
{
	if (buf) free(buf);
}

struct hw_rec_struct_prop *PropCycle::GetNextTrigger()
{
	EventHeaderStruct *evtIn;
	int i, j, n, eptr;

	if (rptr >= head->len) {
		printf("Event after end of cycle requested in PROP\n");
		return NULL;
	}
	evtIn = (EventHeaderStruct *)&buf[rptr];
	evtOut->len = sizeof(struct hw_rec_struct_prop) / sizeof(short) - 1;
	//	board by board
	for(eptr = sizeof(EventHeaderStruct); eptr < evtIn->len - 2; eptr += i+2) {
	    	n = ((buf[rptr + eptr] & 0xF) << 7) | (buf[rptr + eptr + 1] & 0x7F);		// board logical number
	    	for (i=0; i < evtIn->len - eptr - 2; i++) if (buf[rptr + eptr + i + 2] & 0x80) break;	// data length
	    	if (n < 0 || n > 127) continue;							// unknown board - we need number 1 to 12 here
		for (j=0; j<i; j++) StoreHit(n, buf[rptr + eptr + j + 2]);
	}
	rptr += evtIn->len;
	return evtOut;
}

int PropCycle::Read(FILE *f)
{
	int irc;
	int len;

	irc = fread(&len, sizeof(int), 1, f);
	if (irc != 1) {
		if (irc != 0) printf("Strange read of PROP file %m\n");
		return -1;
	}
	if (len < sizeof(RecordHeaderStruct) + sizeof(EventHeaderStruct)) {
		printf("Strange file record of %d bytes found\n", len);
		return -2;
	}
	if (len > size) Realloc(len);
	memcpy(buf, &len, sizeof(int));
	irc = fread(&buf[sizeof(int)], len - sizeof(int), 1, f);
	if (irc != 1) {
		printf("Unexpected EOF - %m\n");
		return -3;
	}
	rptr = sizeof(RecordHeaderStruct);
	if (head->type == REC_CYCLE) return 1;
	return 0;
}

void PropCycle::Realloc(int newsize)
{
	char *tmp;
	
	tmp = (char *)realloc(buf, newsize);
	if (!tmp) {
		printf("No memory: %m\n");
		throw "No memory";
	}
	buf = tmp;
	head = (RecordHeaderStruct *) tmp;
	size = newsize;
}

void PropCycle::StoreHit(int num, int wire)
{
	int i;
	i = evtOut->len - 1;
	if (i >= MAXHITS) {
		printf("Too many hits: cycle %d, Event %d\n", head->number, ((EventHeaderStruct *)&buf[rptr])->number);
		return;
	}
	evtOut->hit[i].wire = wire;
	evtOut->hit[i].brd = num;
	evtOut->len++;
}

/****************************************************************************************************************************************
 *							the main									*
 ****************************************************************************************************************************************/

int main(int argc, char **argv)
{
	FILE *fVME;
	FILE *fPRP;
	FILE *fSYN;
	FILE *fOut;
	PropCycle *prop;
	OutEvent *evt;
	int VMEevent;
	int PRPcycle;
	int cycle, event, num;
	int fnum;
	char str[1024];
	char *ptr;
	int i, irc;
	struct hw_rec_struct_prop *prec;
	int CycleCnt;
	int EventCnt;

	if (argc < 2) {
		printf("Merge DANSS and EPECUR data files with the same run number\n");
		printf("Part IV - merge files.\n");
		printf("Usage: %s NNN\n", argv[0]);
		printf("Will use input files: data/prop_NNN.data, data/vme_NNN.data and data/synclist_NNN.data\n");
		printf("And output file testbench_NNN.data\n");
		return 0;
	}

	fnum = strtol(argv[1], NULL, 10);

	sprintf(str, "data/prop_%3.3d.data", fnum);
	fPRP = fopen(str, "rb");

	sprintf(str, "data/vme_%3.3d.data", fnum);
	fVME = fopen(str, "rb");

	sprintf(str, "data/sync_list_%3.3d.data", fnum);
	fSYN = fopen(str, "rt");

	sprintf(str, "data/testbench_%3.3d.data", fnum);
	fOut = fopen(str, "wb");

	if (!fPRP || !fSYN || !fVME || !fOut) {
		printf("Can not open all required files\n");
		return 10;
	}

	evt = new OutEvent();
	prop = new PropCycle();

	VMEevent = PRPcycle = 0;
	EventCnt = CycleCnt = 0;
	for(;;) {
//		Read syncro information
		ptr = fgets(str, sizeof(str), fSYN);
		if (!ptr) goto fin;
		sscanf(str, "%d %d %d", &cycle, &event, &num);
		for (;PRPcycle<=cycle;) {	// get correct cycle
			irc = prop->Read(fPRP);
			if (irc < 0) {
				goto fin;
			} else if (irc == 1) {
				PRPcycle++;	// point to the next
			}
		}
		for (;VMEevent<event;) {	// skip to the correct event
			irc = evt->Read(fVME);
			if (irc < 0) {
				goto fin;
			} else if (irc == 0) {
				evt->Write(fOut);
				continue;
			}
			VMEevent++;
		}
		for(i=0; i<num; i++) {
			prec = prop->GetNextTrigger();
			irc = evt->Read(fVME);
			if (irc < 0) {
				goto fin;
			} else if (irc == 0) {
				evt->Write(fOut);
				continue;
			}
			if (!prec) {
				printf("*** Cycle %d Prop data format error - trigger counter does not match to real triggers.\n");
				goto fin;
			}
			evt->Add(prec, (prec->len + 1) * sizeof(short));
			evt->Write(fOut);
			EventCnt++;
			VMEevent++;
		}
		CycleCnt++;
	}
fin:
	printf("%d cycles with %d events found.\n", CycleCnt, EventCnt);
	delete prop;
	delete evt;

	if (fVME) fclose(fVME);
	if (fPRP) fclose(fPRP);
	if (fSYN) fclose(fSYN);
	if (fOut) fclose(fOut);
	return 0;
}
