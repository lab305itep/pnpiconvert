/*
	Analize VME + PROP data. Draw tracks, make a tree.
*/
#define _FILE_OFFSET_BITS 64
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <TFile.h>
#include <TTree.h>

#include "recformat-2.h"

#define MAXHIT 5
#define MAXWFD 60
#define MAXCHAN 64

struct WaveFormParamStruct {
	float A;	// amplitude
	float T;	// time
	float I;	// integral
	float Q;	// quality
};

struct TrackParamStruct {
	float AX;	// track X: a*x + b
	float BX;
	float AY;	// trackYX: a*y + b
	float BY;
	float Q;	// quality - negative if bad track in one of the projections
};

union DataParam {
	struct WaveFormParamStruct wave;
	struct TrackParamStruct track;
};

/****************************************************************************************************************************************
 *							Class OutEvent									*
 ****************************************************************************************************************************************/

class VmeEvent {
private:
	char *buf;
	int size;
	struct rec_header_struct *head;
	int ptr;
	void Realloc(int newsize);
	void ProcessWaveForm(short *data, int len, struct WaveFormParamStruct *par);
	void ProcessTrack(struct prop_hit *data, int len, struct TrackParamStruct *par);
	float WeightHits(int *hits, int num);
	float DrawLine(float x1, float x2, float x3, float z1, float z2, float z3, float &a, float &b);
public:
	VmeEvent(void);
	~VmeEvent(void);
	int Read(FILE *f);
	int GetNextRecord(union DataParam *par);
};

VmeEvent::VmeEvent(void)
{
	buf = NULL;
	size = 0;
	head = NULL;
}

VmeEvent::~VmeEvent(void)
{
	if (buf) free(buf);
}


//	Return negative on read error or EOF
//	0 - selftrigger
//	1 - event
//	2 - unknown
int VmeEvent::Read(FILE *f)
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
	ptr = sizeof(struct rec_header_struct);
	if (head->type & REC_EVENT) {
		return 1;
	} else if((head->type & REC_TYPEMASK) == REC_SELFTRIG) {
		return 0;
	} else {
		return 2;
	}
	return 2;
}

void VmeEvent::Realloc(int newsize)
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

//	Get next record of the event.
//	Return -1 on the end of event
//	Return -2 on a record to ignore
//	Return -3 on format error
//	Return record type in the least significant byte (0)
//	Return WFD channel number in byte 1
//	Return WFD module number in bytes 2,3
//	Fill parameters 
int VmeEvent::GetNextRecord(union DataParam *par)
{
	union hw_rec_union *rec;
	int type;
	int module;
	int chan;
	int irc;

	memset(par, 0, sizeof(union DataParam));
	if (!(((head->type & REC_TYPEMASK) == REC_SELFTRIG) || (head->type & REC_EVENT))) return -2;
	if (ptr >= head->len) return -1;
	rec = (union hw_rec_union *) &buf[ptr];
	if (ptr + rec->rec.len > head->len) return -3;
	type = rec->rec.type;
	chan = rec->rec.chan;
	module = rec->rec.module;
	
	switch (type) {
	case TYPE_SELF:
		ProcessWaveForm(rec->self.d, -(rec->rec.len - 2), &par->wave);
		chan = (head->type & REC_CHANMASK) >> 16;
		module = head->type & REC_SERIALMASK;
		irc = (module << 16) | (chan << 8) | type;
		break;
	case TYPE_MASTER:
		ProcessWaveForm(rec->mast.d, rec->rec.len - 2, &par->wave);
		par->wave.T -= 4.0 * rec->mast.time / 3.0;	// fine time correction
		irc = (module << 16) | (chan << 8) | type;
		break;
	case TYPE_PROP:
		ProcessTrack(rec->prop.hit, rec->rec.len - 1, &par->track);
		irc = type;
		break;
	default:
		irc = type;
	}
	ptr += (rec->rec.len + 1) * sizeof(short);
	return irc;
}

void VmeEvent::ProcessWaveForm(short *data, int Len, struct WaveFormParamStruct *par)
{
	int i, len;
	int A;
        float r;
        float b, b2;
        
        if (Len < 0) len = -Len; else len = Len;
        
//	sign extension
	for (i = 0; i < len; i++) if (data[i] & 0x4000) data[i] |= 0x8000;

	// baseline
        b = b2 = 0.;
        if (Len < 0) {
    	    // Calculate baseline with first 15 points (for selftriggers only)
    	    for (i=0; i<15; i++) {
    		b += data[i];
    		b2 += data[i]*data[i];
    	    }
    	    b /= 15.;			// average
    	    b2 = b2 / 15. - b * b;	// rms
    	    
    	    if (b2 > 5) {
    		b = 0.;		// bad baseline
    	    }
        }
        
//	amplitude
	A = -65000;
	for (i=0; i<len; i++) if (A < data[i]) A = data[i];
	par->A = A - b;
//	time
        for (i=0; i<len; i++) if (data[i] > A/2) break;
        if (!i || i == len) {
		par->T = 0;
	} else {
        	r = A;
        	r = (r/2 - data[i-1]) / (data[i] - data[i-1]);
         	par->T = 8.0 * (i + r - 1);
	}
//	integral
	par->I = 0;
	for (i = 0; i < len; i++) par->I += (data[i] - b);
	par->Q = 1. + b2;	// to be done
}

//	Do simple clustering for clean events
//	Single cluster only
float VmeEvent::WeightHits(int *hits, int num)
{
	int i;
	float X;

	if (num <= 0 || num > MAXHIT) return -1000;
	X = 0;
	for(i=0; i<num; i++) X += hits[i];
	X /= num;
	for(i=0; i<num; i++) if (abs(hits[i] - X) > 3) return -1000;
	return X;
}

//	Draw a best line through 3 points
//	return sum of square deviations
float VmeEvent::DrawLine(float x1, float x2, float x3, float z1, float z2, float z3, float &a, float &b)
{
	double sz, sz2, sx, sxz, q;
	
	sz = (z1 + z2 + z3) / 3;
	sx = (x1 + x2 + x3) / 3;
	sz2 = (z1*z1 + z2*z2 + z3*z3) / 3;
	sxz = (x1*z1 + x2*z2 + x3*z3) / 3;
	a = (sxz - sx*sz) / (sz2 - sz*sz);
	b = sx - a * sz;
	q = 0;
	q += (a*z1 + b - x1) * (a*z1 + b - x1);
	q += (a*z2 + b - x2) * (a*z2 + b - x2);
	q += (a*z3 + b - x3) * (a*z3 + b - x3);

	return q;
}

void VmeEvent::ProcessTrack(struct prop_hit *data, int len, struct TrackParamStruct *par)
{
//	Z, mm
	const float X1Z = 0;
	const float X2Z = 1266;
	const float X3Z = 1704;
	const float Y1Z = 12;
	const float Y2Z = 1278;
	const float Y3Z = 1716;
	const float shiftX2 = 0;
	const float shiftY2 = 0;
//	Place for hits
	int nX1, nX2, nX3;
	int nY1, nY2, nY3;
	int hitsX1[MAXHIT], hitsX2[MAXHIT], hitsX3[MAXHIT];
	int hitsY1[MAXHIT], hitsY2[MAXHIT], hitsY3[MAXHIT];
	float X1, X2, X3;
	float Y1, Y2, Y3;
//	
	int i, wire;
//	clear hits
	nX1 = nX2 = nX3 = 0;
	nY1 = nY2 = nY3 = 0;
//	Sort data
	for (i=0; i<len; i++) switch (data[i].brd) {
		// X1
	case 17:
		wire = -100 + 2*data[i].wire;
		if (nX1 < MAXHIT) hitsX1[nX1] = wire;
		nX1++;
		break;
//	case 18:
//		wire = -data[i].wire;
//		if (nX1 < MAXHIT) hitsX1[nX1] = wire;
//		nX1++;
//		break;
		// X2
	case 25:
		wire = 100 - data[i].wire;
		if (nX2 < MAXHIT) hitsX2[nX2] = wire;
		nX2++;
		break;
	case 26:
		wire = -data[i].wire;
		if (nX2 < MAXHIT) hitsX2[nX2] = wire;
		nX2++;
		break;
		// X3
	case 29:
		wire = 100 - data[i].wire;
		if (nX3 < MAXHIT) hitsX3[nX3] = wire;
		nX3++;
		break;
	case 30:
		wire = -data[i].wire;
		if (nX3 < MAXHIT) hitsX3[nX3] = wire;
		nX3++;
		break;
		// Y1
	case 19:
		wire = -100 + 2*data[i].wire;
		if (nY1 < MAXHIT) hitsY1[nY1] = wire;
		nY1++;
		break;
//	case 20:
//		wire = data[i].wire;
//		if (nY1 < MAXHIT) hitsY1[nY1] = wire;
//		nY1++;
//		break;
		// Y2
	case 27:
		wire = -100 + data[i].wire;
		if (nY2 < MAXHIT) hitsY2[nY2] = wire;
		nY2++;
		break;
	case 28:
		wire = data[i].wire;
		if (nY2 < MAXHIT) hitsY2[nY2] = wire;
		nY2++;
		break;
		// Y3
	case 31:
		wire = -100 + data[i].wire;
		if (nY3 < MAXHIT) hitsY3[nY3] = wire;
		nY3++;
		break;
	case 32:
		wire = data[i].wire;
		if (nY3 < MAXHIT) hitsY3[nY3] = wire;
		nY3++;
		break;
	}
//	Process very clean events only
//	reject event if number of hits in any plane > 5
//	reject event if any plane was not hit
	X1 = WeightHits(hitsX1, nX1);
	X2 = WeightHits(hitsX2, nX2);
	X3 = WeightHits(hitsX3, nX3);
	Y1 = WeightHits(hitsY1, nY1);
	Y2 = WeightHits(hitsY2, nY2);
	Y3 = WeightHits(hitsY3, nY3);
	if (X1 < -500 || X2 < -500 || X3 < -500 || Y1 < -500 || Y2 < -500 || Y3 < -500) {
		par->Q = -100;
		return;
	}
//	Correct X2, Y2
	X2 += shiftX2;
	Y2 += shiftY2;
//	Draw linear tracks
	par->Q = 0;
	par->Q += DrawLine(X1, X2, X3, X1Z, X2Z, X3Z, par->AX, par->BX);
	par->Q += DrawLine(Y1, Y2, Y3, Y1Z, Y2Z, Y3Z, par->AY, par->BY);
}

/****************************************************************************************************************************************
 *							the main									*
 ****************************************************************************************************************************************/

int main(int argc, char **argv)
{
	char str[1024];
	char *ptr;
	int fnum;
	FILE *fVME;
	FILE *fConf;
	TFile *fOut;
	VmeEvent *evt;
	union DataParam par;
	TTree *tOut;
	TTree *tSelf[MAXWFD][MAXCHAN];
	struct WaveFormParamStruct SignalData[MAXWFD][MAXCHAN];
	int i, irc, EventCnt;
	int mod, chan;
	struct WaveFormParamStruct SelfData;
	struct TrackParamStruct PropData;
	char cstr[1024];
	int Flags;

	if (argc < 3) {
		printf("Merge DANSS and EPECUR data files with the same run number\n");
		printf("Part V - create a simple root file with hits, selftriggers and tracks.\n");
		printf("Now with baseline subtraction for self triggers.\n");
		printf("Usage: %s sipm.conf NNN | filename.data\n", argv[0]);
		printf("Will use input file: data/testbench_NNN.data\n");
		printf("And output file data/testbench_NNN.root\n");
		printf("If filename.data is given filename.root will be created");
		return 0;
	}

	fConf = fopen(argv[1], "rt");
	if (!fConf) {
		printf("Can not open SiPM config file %s [%m]\n", argv[1]);
		return 50;
	}

	if (isdigit(argv[2][0])) {
		fnum = strtol(argv[2], NULL, 10);
		sprintf(str, "data/testbench_%3.3d.data", fnum);
		fVME = fopen(str, "rb");
		sprintf(str, "data/testbench_%3.3d.root", fnum);
		fOut = new TFile(str, "RECREATE");
	} else {
		fVME = fopen(argv[2], "rb");
		strcpy(str, argv[2]);
		ptr = strrchr(str, '.');
		if (ptr) *ptr = '\0';
		strcat(str, ".root");
	}
	fOut = new TFile(str, "RECREATE");
	
	if (!fVME || !fOut->IsOpen()) {
		printf("Can not open all required files\n");
		return 10;
	}

	evt = new VmeEvent();

	tOut = new TTree("Event", "Event");
	tOut->Branch("Track", &PropData, "AX/F:BX/F:AY/F:BY/F:ABQ/F");
	tOut->Branch("Flags", &Flags, "Flags/i");
	memset(tSelf, 0, sizeof(tSelf));
	for (;!feof(fConf);) {
		if (!fgets(cstr, sizeof(cstr), fConf)) break;
		if (strlen(cstr) == 0 || cstr[0] == '*') continue;
		sscanf(cstr, "%d.%d", &mod, &chan);
		if (mod <= 0 || mod >= MAXWFD || chan < 0 || chan >= MAXCHAN) {
			printf("Bad string in the configuration file: %s\n", cstr);
			continue;
		}
		sprintf(str, "Self_%2.2d_%2.2d", mod, chan);
		tSelf[mod][chan] = new TTree(str, str);
		tSelf[mod][chan]->Branch("Self", &SelfData, "A/F:T/F:I/F:Q/F");
		sprintf(str, "SiPM_%2.2d_%2.2d", mod, chan);
		tOut->Branch(str, &SignalData[mod][chan], "A/F:T/F:I/F:Q/F");
	}
	fclose(fConf);

	EventCnt = 0;
	for(;;) {
		irc = evt->Read(fVME);
		if (irc < 0) break;
		switch (irc) {
		case 0:		// Self triggers
			irc = evt->GetNextRecord(&par);
			mod = (irc >> 16) & 0xFFFF;
			chan = (irc >> 8) & 0x3F;
			if (mod <= 0 || mod >= MAXWFD) break;
			if (!tSelf[mod][chan]) continue;
			memcpy(&SelfData, &par.wave, sizeof(struct WaveFormParamStruct));
			tSelf[mod][chan]->Fill();
			break;
		case 1:
			memset(&SignalData[0][0], 0, sizeof(SignalData));
			memset(&PropData, 0, sizeof(PropData));
			Flags = 0;
			for (;;) {
				irc = evt->GetNextRecord(&par);
				if (irc < 0) break;
				mod = (irc >> 16) & 0xFFFF;
				chan = (irc >> 8) & 0x3F;
				if ((irc & 0xFF) < 32) Flags |= 1 << (irc & 0xFF);
				switch(irc & 0xFF) {
				case TYPE_MASTER:
					if (mod <= 0 || mod >= MAXWFD) break;
					memcpy(&SignalData[mod][chan], &par.wave, sizeof(struct WaveFormParamStruct));
					break;
				case TYPE_PROP:
					memcpy(&PropData, &par.track, sizeof(struct TrackParamStruct));
					break;
				}
			}
			tOut->Fill();
			EventCnt++;
			break;
		}
	}
fin:
	printf("%d events found.\n", EventCnt);
	delete evt;

	if (fVME) fclose(fVME);
	if (fOut) {
		fOut->cd();
		tOut->Write();
		for (mod = 0; mod < MAXWFD; mod++) for (chan = 0; chan < MAXCHAN; chan++)  if (tSelf[mod][chan]) tSelf[mod][chan]->Write();
		fOut->Close();
	}
	return 0;
}
