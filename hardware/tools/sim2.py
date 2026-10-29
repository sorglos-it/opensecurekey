import json,math
LIMIT=0.15200122; OUTL=0.29999940; BW=27.0
F='mcp-EasyEDA_Pro-pcb_pads_list-1792925226727.txt'
t=open(F,encoding='utf-8',errors='replace').read()
pads=json.loads(t[t.find('{'):t.rfind('}')+1])['result']['pads']
ALT={"C15":(10.499999,55.500001),"C16":(16.5,55.500001),
     "R3":(19.2408,2.000001),"R4":(20.9243,2.000001),"R5":(22.605299,2.000001),
     "R13":(24.286201,2.000001),"R14":(25.967201,2.000001),"C10":(19.6,2.949)}
NEU={"C15":(21.5,12.6,0),"C16":(21.5,13.5,180),
     "R3":(19.2408,0.9,0),"R4":(20.9243,0.9,0),"R5":(22.605299,0.9,0),
     "R13":(24.286201,0.9,0),"R14":(25.967201,0.9,0),"C10":(22.40026,1.95,0)}
def pos(p):
    d=p['designator']
    if d not in NEU: return (p['x_mm'],p['y_mm'])
    ox,oy=ALT[d]; nx,ny,rot=NEU[d]
    dx,dy=p['x_mm']-ox,p['y_mm']-oy
    if rot==180: dx,dy=-dx,-dy
    return (nx+dx,ny+dy)
def rect(p):
    ex,ey=p['extent_mm']; x,y=pos(p); return (x-ex/2,x+ex/2,y-ey/2,y+ey/2)
def gap(a,b):
    dx=max(a[0]-b[1],b[0]-a[1]); dy=max(a[2]-b[3],b[2]-a[3])
    return -min(-dx,-dy) if (dx<0 and dy<0) else math.hypot(max(dx,0),max(dy,0))
lay=lambda L:{'TOP','BOTTOM'} if L=='MULTI' else {L}
PARK={'J2','J4','J5','J6'}
it=[(p['designator'],p['padNumber'],p.get('net','') or '',rect(p),lay(p['layer'])) for p in pads]
bad=[]
for i in range(len(it)):
    for j in range(i+1,len(it)):
        a,b=it[i],it[j]
        if a[0]==b[0] or not (a[4]&b[4]) or a[0] in PARK or b[0] in PARK: continue
        g=gap(a[3],b[3])
        if g<LIMIT: bad.append((g,a,b,bool(a[2]) and a[2]==b[2]))
print("Paare unter %.4f: %d" % (LIMIT,len(bad)))
for g,a,b,s in sorted(bad):
    print("   %s.%-3s [%-9s] <-> %s.%-3s [%-9s] %+8.4f%s" % (a[0],a[1],a[2] or '-',b[0],b[1],b[2] or '-',g,'  gleiches Netz' if s else ''))
rb=[(d,pn,round(r[2],3)) for d,pn,n,r,L in it if d not in PARK and (r[0]<OUTL or r[1]>BW-OUTL or r[2]<OUTL)]
print("Pads unter %.3f am Rand (x oder y=0): %d %s" % (OUTL,len(rb),rb))
zx0,zx1,zy0,zy1=23.1484,27.0004,12.0499,22.5499
inz=[(d,pn) for d,pn,n,r,L in it if d not in PARK|{'U1'} and r[0]<zx1 and r[1]>zx0 and r[2]<zy1 and r[3]>zy0]
print("fremde Pads in der Freihaltezone: %d %s" % (len(inz),inz))
P={(p['designator'],p['padNumber']):p for p in pads}
for cap,ziel in (("C15",("U1","52")),("C16",("U1","54")),("C10",("U2","12"))):
    k=[kk for kk in P if kk[0]==cap and (P[kk].get('net') or '')!='GND'][0]
    print("   %-4s -> %s.%-3s  %6.2f mm" % (cap,ziel[0],ziel[1],math.dist(pos(P[k]),(P[ziel]['x_mm'],P[ziel]['y_mm']))))
