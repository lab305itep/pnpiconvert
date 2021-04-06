dat2rootp : dat2rootp.cpp
	g++ -o $@ $^ `root-config --cflags --glibs`
