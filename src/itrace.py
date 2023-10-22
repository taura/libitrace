#!/usr/bin/python3
import os,mmap,re,subprocess,sys,io
import pdb

def get_this_file():
    g = globals()
    file = None
    if __name__ == "__main__" and \
       len(sys.argv) > 0 and len(sys.argv[0]) > 0:
        # run from command line (python .../gxpd.py)
        file = sys.argv[0]
    elif g.has_key("__file__"):
        # appears this has been loaded as a module
        file = g["__file__"]
        # if it is .pyc file, get .py instead
        m = re.match("(.+)\.pyc$", file)
        if m: file = m.group(1) + ".py"
    if file is None:
        return None,"cannot find the location of this_file.py"
    #file = os.path.abspath(file)
    file = os.path.realpath(file)
    if os.access(file, os.F_OK) == 0:
        return None,("source file %s is missing!\n" % file)
    elif os.access(file, os.R_OK) == 0:
        return None,("cannot read source file %s!\n" % file)
    else:
        return file,None

def get_this_dir():
    f,err = get_this_file()
    if f is None:
        return None,err
    else:
        a,b = os.path.split(f)
        return a,None
    
class shared_object:
    def __init__(self, start, end, path):
        self.start = start
        self.end = end
        self.path = path

class mem_map:
    def __init__(self, objects):
        self.objects = objects
    def find_addr(self, addr):
        for o in self.objects:
            if o.start <= addr < o.end:
                return o.path,(addr - o.start)
        return None,None

class self_maps_parser:
    def __init__(self, filename):
        self.so_pat = re.compile("(?P<start>[0-9a-f]+)\-(?P<end>[0-9a-f]+) [rwxps\-]{4} [0-9a-f]+ [0-9a-f:]+ [0-9]+ +(?P<path>.*)")
        self.addr_pat = re.compile("query: (?P<addr>[0-9a-fx]+)")
        self.fp = open(filename, "r")
        self.nextline()

    def nextline(self):
        self.cur = self.fp.readline()
    
    def header(self):
        assert(self.cur == "***** begin maps *****\n"), self.cur
        self.nextline()

    def trailer(self):
        assert(self.cur == "***** end maps *****\n"), self.cur
        self.nextline()

    def so_line(self):
        """
        563dce55b000-563dce55c000 r-xp 00000000 08:01 12851267                   /home/tau/tmp/bfd/a.out
        """
        m = self.so_pat.match(self.cur)
        if m:
            s,e,p = m.group("start", "end", "path")
            self.nextline()
            return shared_object(int(s, 16), int(e, 16), p)
        else:
            return None

    def maps(self):
        # self.header()
        objects = []
        while 1:
            so = self.so_line()
            if so is None: break
            objects.append(so)
        # self.trailer()
        return mem_map(objects)

def safe_int(s):
    try:
        return int(s)
    except ValueError:
        return s

def safe_decode(s):
    return s.decode("utf-8", "replace")

def addr2line(path, addrs_counts):
    # print("resolving %d addresses for %s" % (len(addrs_counts), path))
    p = subprocess.Popen([ "addr2line", "-e", path, "-f", "-C" ],
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE)
    S = {}
    for i,(offset,count) in enumerate(addrs_counts):
        p.stdin.write(b"0x%x\n" % offset)
        p.stdin.flush()
        fun = p.stdout.readline()
        assert(fun)
        line = p.stdout.readline()
        src_lineno = line.rsplit(b":", 1)
        assert(len(src_lineno) == 2), line
        [ src,lineno_s ] = src_lineno
        if src and safe_decode(src) != "??":
            lineno0 = lineno_s.split()[0]
            lineno = int(lineno0)
        else:
            lineno = 0
        if src not in S:
            S[src] = []
        S[src].append((lineno, fun, count))
    p.stdin.close()
    p.wait()
    return S
    
def parse_itrace_log(fp):
    header_pat = re.compile("(?P<key>[_A-Za-z]+): (?P<val>.*)")
    addr_pat = re.compile("\[(?P<i>\d+)\]: a: (?P<addr>0x[0-9a-f]+) count: (?P<count>\d+)")
    H = {}                      # header
    R = {}                      # records
    U = []                      # unresolved addresses
    line = safe_decode(fp.readline())
    while line:
        m = header_pat.match(line)
        if m is None: break
        k,vs = m.group("key", "val")
        H[k] = safe_int(vs)
        line = safe_decode(fp.readline())
    p = self_maps_parser(H["maps"])
    sm = p.maps()
    while line:
        m = addr_pat.match(line)
        assert(m), line
        # if m is None: break
        i,addr_s,count_s = m.group("i", "addr", "count")
        addr = int(addr_s, 16)
        count = int(count_s)
        path,offset = sm.find_addr(addr)
        line = safe_decode(fp.readline())
        if path:
            if path not in R:
                R[path] = []
            R[path].append((offset, count))
        else:
            U.append((addr, count))
    fp.close()
    P = {}
    for path,A in sorted(R.items()):
        print("path %s" % path.strip())
        S = addr2line(path, A)
        for src,line_fun_counts in sorted(S.items()):
            print(" src %s" % safe_decode(src).strip())
            for lineno,fun,count in sorted(line_fun_counts):
                print("  fun %5d %5d %s"
                      % (lineno, count, safe_decode(fun).strip()))
                
def main():
    di,err = get_this_dir()
    if di is None:
        sys.stderr.write("%s\n" % err)
        return 1
    cmd = sys.argv[1]
    itrace_log = sys.argv[2]
    # itrace_log = "test/itrace_21360.log"
    itrace_ctl = "%s/itrace_ctl" % di
    if cmd == "show":
        p = subprocess.Popen([ itrace_ctl, "show", itrace_log ],
                             stdout=subprocess.PIPE)
        parse_itrace_log(p.stdout)
        return p.returncode
    elif cmd == "start" or cmd == "stop":
        p = subprocess.run([ itrace_ctl, cmd, itrace_log ])
        return p.returncode
    else:
        sys.stderr.write("usage:\n  %s [show|start|stop] itrace_PID.dat\n" % sys.argv[0])
        return 1
    return 0
    
sys.exit(main())
