# A small Joliet ISO for tests/dc_iso_test.c: a DATA folder of 602 files
# (sizes 1..300000 bytes, random bytes) spanning many directory sectors,
# written to OUT.iso with OUT.json (name -> [sector, size]).
#   python3 tests/dc_iso_mkiso.py OUT
import struct, random, sys
random.seed(3)
S=2048
def both16(v): return struct.pack('<H',v)+struct.pack('>H',v)
def both32(v): return struct.pack('<I',v)+struct.pack('>I',v)
def rec(name_bytes, lba, size, isdir):
    n=len(name_bytes); l=33+n+((n+1)%2)
    r=bytes([l,0])+both32(lba)+both32(size)+bytes(7)+bytes([2 if isdir else 0,0,0])+both16(1)+bytes([n])+name_bytes
    return r+bytes(l-len(r))
files=[('CORE.DAT',5000)]+[('F%03d.DAT'%i, random.randint(1,70000)) for i in range(600)]+[('BIG.WAV',300000)]
# layout: 0-15 system, 16 PVD, 17 SVD(joliet), 18 terminator, 19.. dirs
def dir_bytes(entries, joliet, selflba, selfsize, parentlba, parentsize):
    recs=[rec(b'\x00',selflba,selfsize,True), rec(b'\x01',parentlba,parentsize,True)]
    for name,lba,size,isdir in entries:
        nb = name.encode('utf-16-be') if joliet else (name+('' if isdir else ';1')).encode()
        recs.append(rec(nb,lba,size,isdir))
    out=b''; cur=b''
    for r in recs:
        if len(cur)+len(r)>S: out+=cur+bytes(S-len(cur)); cur=b''
        cur+=r
    out+=cur+bytes((S-len(cur))%S)
    return out
# sizes first with placeholder lbas
def nsec(b): return (len(b)+S-1)//S
data_start=200
lba=data_start; flist=[]
contents={}
for n,sz in files:
    flist.append((n,lba,sz,False)); contents[n]=bytes(random.randrange(256) for _ in range(sz)); lba+=(sz+S-1)//S
total=lba+10
img=bytearray(total*S)
for j,base in ((0,19),(1,100)):
    joliet=j==1
    # data dir after root
    dd=dir_bytes(sorted(flist), joliet, 0,0,0,0); ds=nsec(dd)
    root_lba=base; data_lba=base+1
    rd=dir_bytes([('data' if joliet else 'DATA',data_lba,ds*S,True)], joliet, root_lba,S,root_lba,S)
    dd=dir_bytes(sorted(flist), joliet, data_lba,ds*S,root_lba,S)
    img[root_lba*S:root_lba*S+len(rd)]=rd; img[data_lba*S:data_lba*S+len(dd)]=dd
    vd=bytearray(S); vd[0]=2 if joliet else 1; vd[1:6]=b'CD001'; vd[6]=1
    if joliet: vd[88:91]=b'%/E'
    vd[156:156+34]=rec(b'\x00',root_lba,S,True)
    img[(17 if joliet else 16)*S:(18 if joliet else 17)*S]=vd
term=bytearray(S); term[0]=255; term[1:6]=b'CD001'; img[18*S:19*S]=term
for n,l,sz,_ in flist: img[l*S:l*S+sz]=contents[n]
open(sys.argv[1]+'.iso','wb').write(img)
import json; json.dump({n:[l,sz] for n,l,sz,_ in flist}, open(sys.argv[1]+'.json','w'))
print('ok', total, 'sectors; data dir sectors', ds)
