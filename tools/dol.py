import struct,sys
d=open(sys.argv[1],"rb").read()
off=struct.unpack(">18I",d[0:72]);addr=struct.unpack(">18I",d[72:144]);sz=struct.unpack(">18I",d[144:216])
def rd(a):
  for i in range(18):
    if sz[i] and addr[i]<=a<addr[i]+sz[i]: return struct.unpack(">I",d[off[i]+a-addr[i]:off[i]+a-addr[i]+4])[0]
def dis(a,n):
  for k in range(n):
    w=rd(a+4*k); op=w>>26; rd_=(w>>21)&31; ra=(w>>16)&31; imm=w&0xffff
    s=hex(w)
    if op==18: li=w&0x3fffffc; li=li-(1<<26) if li&0x2000000 else li; s="b%s %s"%("l" if w&1 else "",hex(a+4*k+li))
    elif op==15: s="addis r%d,r%d,%s"%(rd_,ra,hex(imm))
    elif op==24: s="ori r%d,r%d,%s"%(ra,rd_,hex(imm))
    elif op==14: s="addi r%d,r%d,%d"%(rd_,ra,imm-(1<<16) if imm&0x8000 else imm)
    print(hex(a+4*k),s)
dis(int(sys.argv[2],16),int(sys.argv[3]))
