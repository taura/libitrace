CXX := g++
CXXFLAGS := -O0 -g -finstrument-functions
so := libf.so libg.so libh.so
so_src := $(patsubst lib%.so,%.cc,$(so))
exe := q
src := $(addsuffix,.cc,$(exe))
all : $(exe) $(exe)_static

$(exe) : % : %.cc $(so)
	$(CXX) -o $@ $(CXXFLAGS) $< -L. -Wl,-R. -lf -lg -lh

$(so) : lib%.so : %.cc
	$(CXX) -o $@ $(CXXFLAGS) -shared -fPIC $<

$(exe)_static : %_static : %.cc $(so_src)
	$(CXX) -o $@ $(CXXFLAGS) $< $(so_src)

clean :
	rm -f $(so) $(exe) $(exe)_static gmon.out
