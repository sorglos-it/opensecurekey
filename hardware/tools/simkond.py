import json,math,collections
LIMIT=0.15200122; OUTL=0.29999940; BW=27.0
F='mcp-EasyEDA_Pro-pcb_pads_list-1792924886803.txt'
t=open(F,encoding='utf-8',errors='replace').read()
pads=json.loads(t[t.find('{'):t.rfind('}')+1])['result']['pads']
ALT={"C1":(20.8008,8.300001),"C4":(5.9689,23.9),"C8":(7.561,23.9),"C9":(9.150599,23.9),
     "C10":(19.499999,7.000001),"C11":(21.092101,7.000001),"C12":(3.500001,21.000001),
     "C17":(24,9.499999)}
NEU={"C1":(2.0,8.0,0),"C4":(12.469,14.78,0),"C8":(10.57,22.3,0),"C9":(13.07,22.3,0),
     "C10":(19.6,2.949,180),"C11":(19.2,4.50088,180),"C12":(10.85,13.59408,0),
     "C17":(11.63,18.90014,180)}
def pos(p):
    d=p['designator']
    if d not in NEU: return (p['x_mm'],p['y_mm'])
    ox,oy=ALT[d]; nx,ny,rot=NEU[d]
    dx,dy=p['x_mm']-ox,p['y_mm']-oy
    if rot==180: dx,dy=-dx,-dy
    return (nx+dx,ny+dy)
def rect(p):
    ex,ey=p['extent_mm']; x,y=pos(p)
    return (x-ex/2,x+ex/2,y-ey/2,y+ey/2)
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
print("Paare unter %.4f nach dem Umsetzen: %d" % (LIMIT,len(bad)))
for g,a,b,s in sorted(bad):
    print("   %s.%-3s [%-10s] <-> %s.%-3s [%-10s] %+8.4f%s" % (a[0],a[1],a[2] or '-',b[0],b[1],b[2] or '-',g,'  gleiches Netz' if s else ''))
rb=[(d,pn) for d,pn,n,r,L in it if d not in PARK and (r[0]<OUTL or r[1]>BW-OUTL)]
print("Pads unter %.3f am Rand: %d %s" % (OUTL,len(rb),rb))
zx0,zx1,zy0,zy1=23.1484,27.0004,12.0499,22.5499
inz=[(d,pn) for d,pn,n,r,L in it if d not in PARK|{'U1'} and r[0]<zx1 and r[1]>zx0 and r[2]<zy1 and r[3]>zy0]
print("fremde Pads in der Freihaltezone: %d %s" % (len(inz),inz))
P={(p['designator'],p['padNumber']):p for p in pads}
Z={"C1":("U4","4"),"C4":("U1","30"),"C8":("X1","1"),"C9":("X1","2"),"C10":("U2","12"),
   "C11":("U2","18"),"C12":("U3","8"),"C17":("U1","20")}
print()
print("neue Abstaende:")
for cap,ziel in Z.items():
    k=[kk for kk in P if kk[0]==cap and (P[kk].get('net') or '')!='GND'][0]
    d=math.dist(pos(P[k]),(P[ziel]['x_mm'],P[ziel]['y_mm']))
    print("   %-4s -> %s.%-3s  %6.2f mm" % (cap,ziel[0],ziel[1],d))
