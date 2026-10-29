import json,math,sys
t=open(sys.argv[1],encoding='utf-8',errors='replace').read()
pads=json.loads(t[t.find('{'):t.rfind('}')+1])['result']['pads']
P={(p['designator'],p['padNumber']):p for p in pads}
def c(k): return (P[k]['x_mm'],P[k]['y_mm'])
def lay(k): return P[k]['layer']
# Kondensator -> (Ziel-Pad, Quelle in schematic.md)
Z=[("C1",("U4","4"),"Z. 99  U4 Pin 4"),
   ("C2",("U4","3"),"Z. 101 U4 Pin 3"),
   ("C3",("U1","30"),"Z. 165 VSYS"),
   ("C4",("U1","30"),"Z. 165 VSYS"),
   ("C5",("U1","28"),"Z. 166 U1 VDD"),
   ("C8",("X1","1"),"Z. 188 X1 Pin 1"),
   ("C9",("X1","2"),"Z. 189 X1 Pin 2"),
   ("C10",("U2","12"),"Z. 232 nahe Pin 12"),
   ("C11",("U2","18"),"Z. 233 nahe Pin 18"),
   ("C12",("U3","8"),"Z. 313 nahe Pin 8"),
   ("C13",("Q4","3"),"Z. 357 VDD_OLED"),
   ("C14",("Q4","3"),"Z. 357 VDD_OLED"),
   ("C15",("U1","52"),"Z. 555 P0.09/NFC1"),
   ("C16",("U1","54"),"Z. 556 P0.10/NFC2"),
   ("C17",("U1","20"),"Z. 696 VBAT_SENSE")]
print("%-4s %-9s %-7s %-10s %-9s %8s   %s" % ("Kond","Netz","Lage","Ziel","Lage","Abstand","Quelle"))
print("-"*86)
rows=[]
for cap,ziel,q in Z:
    # das Pad des Kondensators, das NICHT an GND liegt
    kand=[k for k in P if k[0]==cap]
    sig=[k for k in kand if (P[k].get('net') or '')!='GND']
    k = sig[0] if sig else kand[0]
    d=math.dist(c(k),c(ziel))
    rows.append((d,cap,P[k].get('net') or '-',lay(k),"%s.%s"%ziel,lay(ziel),q))
for d,cap,net,l1,z,l2,q in sorted(rows):
    mark="" if d<=2.0 else ("   <-- weit" if d<=6 else "   <-- SEHR WEIT")
    print("%-4s %-9s %-7s %-10s %-9s %8.2f   %s%s" % (cap,net,l1,z,l2,d,q,mark))
