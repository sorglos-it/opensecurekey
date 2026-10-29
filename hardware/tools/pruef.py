import json,math,sys
LIMIT=0.15200122; OUTL=0.29999940; BW=27.0
F=sys.argv[1]
t=open(F,encoding='utf-8',errors='replace').read()
pads=json.loads(t[t.find('{'):t.rfind('}')+1])['result']['pads']
def rect(p):
    ex,ey=p['extent_mm']; return (p['x_mm']-ex/2,p['x_mm']+ex/2,p['y_mm']-ey/2,p['y_mm']+ey/2)
def gap(a,b):
    dx=max(a[0]-b[1],b[0]-a[1]); dy=max(a[2]-b[3],b[2]-a[3])
    return -min(-dx,-dy) if (dx<0 and dy<0) else math.hypot(max(dx,0),max(dy,0))
lay=lambda L: {'TOP','BOTTOM'} if L=='MULTI' else {L}
PARK={'J2','J5','J6'}
it=[(p['designator'],p['padNumber'],p.get('net','') or '',rect(p),lay(p['layer'])) for p in pads]
bad=[];mind=(9e9,None,None)
for i in range(len(it)):
    for j in range(i+1,len(it)):
        a,b=it[i],it[j]
        if a[0]==b[0] or not (a[4]&b[4]): continue
        if a[0] in PARK or b[0] in PARK: continue
        g=gap(a[3],b[3]); same=bool(a[2]) and a[2]==b[2]
        if g<mind[0]: mind=(g,a,b)
        if g<LIMIT: bad.append((g,a,b,same))
print("Pads: %d   Paare unter %.6f: %d" % (len(pads),LIMIT,len(bad)))
for g,a,b,s in sorted(bad):
    print("   %s.%-4s [%-10s] <-> %s.%-4s [%-10s] %+8.4f%s" % (a[0],a[1],a[2] or '-',b[0],b[1],b[2] or '-',g,'  gleiches Netz' if s else ''))
print("kleinster Abstand ueberhaupt: %.4f mm (%s.%s [%s] <-> %s.%s [%s])" %
      (mind[0],mind[1][0],mind[1][1],mind[1][2],mind[2][0],mind[2][1],mind[2][2]))
rb=[(d,pn,round(r[0],4),round(r[1],4)) for d,pn,n,r,L in it if d not in PARK and (r[0]<OUTL or r[1]>BW-OUTL)]
print("Pads unter %.3f am Rand: %d %s" % (OUTL,len(rb),rb))
zx0,zx1,zy0,zy1=23.1484,27.0004,12.0499,22.5499
inz=[(d,pn,n,[round(v,4) for v in r]) for d,pn,n,r,L in it if d not in PARK|{'U1'} and r[0]<zx1 and r[1]>zx0 and r[2]<zy1 and r[3]>zy0]
print("Fremde Pads in der Freihaltezone: %d %s" % (len(inz),inz))
sw=[ (pn,[round(v,4) for v in r]) for d,pn,n,r,L in it if d=='SW2']
print("SW2 jetzt:", sorted(sw))
