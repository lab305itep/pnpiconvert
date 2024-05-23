All : dat2rootp makelist_prop pconvert makelist_sync makelist_vme

clean : 
	-rm -f dat2rootp makelist_prop pconvert makelist_sync makelist_vme

dat2rootp : dat2rootp.cpp
	g++ -o $@ $^ `root-config --cflags --glibs`
