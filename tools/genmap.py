import struct,sys
d=open(sys.argv[1],"rb").read()
off=struct.unpack(">18I",d[0:72]);addr=struct.unpack(">18I",d[72:144]);sz=struct.unpack(">18I",d[144:216])
text=[(off[i],addr[i],sz[i]) for i in range(7) if sz[i]]
data=[(off[i],addr[i],sz[i]) for i in range(7,18) if sz[i]]
W={}
for o,a,n in text:
  for k,x in enumerate(struct.unpack(">%dI"%(n//4),d[o:o+n])): W[a+4*k]=x
def is_end(x): return x==0x4e800020 or (x>>26)==18 and not x&1 or x==0 or x==0x4e800420
def is_pro(x): return (x>>16)==0x9421 or x==0x7c0802a6
starts=set(); why={}
for a,x in W.items():
  if (x>>26)==18 and x&1 and not x&2:
    li=x&0x3fffffc; li=li-(1<<26) if li&0x2000000 else li
    t=a+li
    if t in W: starts.add(t)
nb=len(starts)
for o,a,n in data:
  for k,p in enumerate(struct.unpack(">%dI"%(n//4),d[o:o+n-(n%4)])):
    if p in W and p%4==0 and (is_pro(W[p]) or (p-4 in W and is_end(W[p-4]))): starts.add(p)
nd=len(starts)
for a,x in W.items():
  if is_end(x) and x!=0 and a+4 in W and is_pro(W[a+4]): starts.add(a+4)
for o,a,n in text: starts.add(a)
print("bl targets",nb,"+data ptrs",nd-nb,"+prologue after end",len(starts)-nd,"total",len(starts),file=sys.stderr)
for s in sorted(starts): print("%08x 0x%08x"%(s,s))
