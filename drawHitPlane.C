void drawHitPlane(TTree *evt, const char *what = "anystrip", const double Z = 1450)
{
	char strval[1024];
	int i, j;
	
	TString str("ABQ >= 0 && ABQ < 100");
	if (what[0] == 'h' || what[0] == '?' || what == 'H') {
		printf("Draw XY plane at Z for particular SiPM hit combination\n");
		printf("drawHitPlane(TTree *, const char *combination, const double Z = 1450\n");
		printf("Possible combinations:\n");
		printf("help - print this message\n");
		printf("none - any track ignoring SiPM\n");
		printf("SiPM_MM_NN - hit in SiPM MM.NN\n");
		printf("anystrip - hit in any SiPM of the strip configuration\n");
		printf("anyaquarium - hit in any SiPM of the aquarium configuration\n");
		printf("anycube - hit in any SiPM of the cube configuration\n");
		return;
	} else if (what[0] == 'S' && what[1] == 'i') {	// this is a single SiPM name like SiPM_53_01 expected
		sprintf(strval, " && %s.A > 200", what);
		str = str + strval;
	} else if (TString(what) == "none") {		// ignore strips
		// do nothing
	} else if (TString(what) == "anystrip") {	// for Strips
		str = str + " && (";
		for (i = 0; i<4; i++) for (j=0; j<8; j++) {
			if (16 * i + j > 0) str = str + " || ";
			sprintf(strval, "SiPM_53_%2.2d.A > 200", 16 * i + j);
			str = str + strval;
			sprintf(strval, " || SiPM_47_%2.2d.A > 200", 16 * i + j);
			str = str + strval;
		}
		str = str + ")";
	} else if (TString(what) == "anyaquarium") {	// for Aquarium
		str = str + " && (";
		for (i = 0; i<2; i++) for (j=0; j<8; j++) {
			if (16 * i + j > 0) str = str + " || ";
			sprintf(strval, "SiPM_53_%2.2d.A > 200", 16 * i + j);
			str = str + strval;
		}
		str = str + ")";
	} else if (TString(what) == "anycube") {	// for Cubes
		str = str + " && (";
		for (i = 0; i<2; i++) for (j=0; j<15; j++) {
			if (16 * i + j > 0) str = str + " || ";
			sprintf(strval, "SiPM_53_%2.2d.A > 200", 16 * i + j);
			str = str + strval;
		}
		str = str + ")";
	} else {
		printf("Unknown mode %s\n", what);
		return;
	}
	sprintf(strval, "AY*%f+BY:AX*%f+BX", Z, Z);
	printf("%s [%s]\n", strval, str.Data());
	evt->Draw(strval, str.Data());
}
