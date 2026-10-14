#!/usr/bin/env python3
# NV2A vertex program microcode -> readable asm (field layout: xemu hw/xbox/nv2a/vsh.c).
# Input: lines with "<index>: w0 w1 w2 w3" (the XB_DUMP_FRAME log); constants as c[-96..95].
# Usage: grep "vs <key> *[0-9]*:" log | tools/nv2a_vsdis.py
import sys,re
MAC=['NOP','MOV','MUL','ADD','MAD','DP3','DPH','DP4','DST','MIN','MAX','SLT','SGE','ARL']
ILU=['NOP','MOV','RCP','RCC','RSQ','EXP','LOG','LIT']
OUT={0:'oPos',3:'oD0',4:'oD1',5:'oFog',6:'oPts',7:'oB0',8:'oB1',9:'oT0',10:'oT1',11:'oT2',12:'oT3'}
def g(w,d,s,b): return (w[d]>>s)&((1<<b)-1)
def sw(x,y,z,ww):
    c='xyzw'; s=c[x]+c[y]+c[z]+c[ww]
    return '' if s=='xyzw' else '.'+(s[0] if len(set(s))==1 else s)
def opnd(w,mux,r,neg,s,rel):
    if mux==1: n='R%d'%r
    elif mux==2: n='v%d'%g(w,1,9,4)
    elif mux==3: n='c[%s%d]'%('A0+' if rel else '',g(w,1,13,8)-96)
    else: n='?'
    return ('-' if neg else '')+n+s
def mask(m): return '.'+''.join(c for c,b in zip('xyzw',[8,4,2,1]) if m&b)
for line in sys.stdin:
    m=re.search(r'(\d+): ([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8})',line)
    if not m: continue
    w=[int(m.group(i),16) for i in range(2,6)]
    rel=g(w,3,1,1)
    A=opnd(w,g(w,2,26,2),g(w,2,28,4),g(w,1,8,1),sw(g(w,1,6,2),g(w,1,4,2),g(w,1,2,2),g(w,1,0,2)),rel)
    B=opnd(w,g(w,2,11,2),g(w,2,13,4),g(w,2,25,1),sw(g(w,2,23,2),g(w,2,21,2),g(w,2,19,2),g(w,2,17,2)),rel)
    C=opnd(w,g(w,3,28,2),g(w,2,0,2)<<2|g(w,3,30,2),g(w,2,10,1),sw(g(w,2,8,2),g(w,2,6,2),g(w,2,4,2),g(w,2,2,2)),rel)
    mac=g(w,1,21,4); ilu=g(w,1,25,3)
    outr=g(w,3,20,4); macm=g(w,3,24,4); ilum=g(w,3,16,4); om=g(w,3,12,4); orb=g(w,3,11,1); oaddr=g(w,3,3,8); omux=g(w,3,2,1)
    parts=[]
    nargs={'MOV':[A],'MUL':[A,B],'ADD':[A,C],'MAD':[A,B,C],'DP3':[A,B],'DPH':[A,B],'DP4':[A,B],'DST':[A,B],'MIN':[A,B],'MAX':[A,B],'SLT':[A,B],'SGE':[A,B],'ARL':[A]}
    o = (OUT.get(oaddr,'o%d'%oaddr) if orb else 'c[%d]'%(oaddr-96))+mask(om) if om else None
    if mac:
        dsts=[]
        if macm: dsts.append('R%d'%outr+mask(macm))
        if o and not omux: dsts.append(o)
        if MAC[mac]=='ARL': dsts=['A0']
        parts.append('%s %s, %s'%(MAC[mac],'+'.join(dsts) or '-',', '.join(nargs[MAC[mac]])))
    if ilu:
        dsts=[]
        if ilum: dsts.append('R%d'%(1 if mac else outr)+mask(ilum))
        if o and omux: dsts.append(o)
        parts.append('%s %s, %s'%(ILU[ilu],'+'.join(dsts) or '-',C))
    print('%s: %s%s'%(m.group(1),' ; '.join(parts),'  [final]' if w[3]&1 else ''))
