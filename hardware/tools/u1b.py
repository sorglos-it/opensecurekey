# gemessene Huelle (aus den 61 Pads)
px0,px1,py0,py1 = 12.0992, 23.4999, 12.2998, 22.3000
# Koerpermass aus dem Datenblatt
BX,BY = 15.5, 10.5
print("gemessen  Padhuelle : x %.4f ... %.4f = %.4f   y %.4f ... %.4f = %.4f"
      % (px0,px1,px1-px0,py0,py1,py1-py0))
inset = (BY-(py1-py0))/2
print("Pad-Einzug an den Langseiten: (%.2f - %.4f)/2 = %.4f mm" % (BY,py1-py0,inset))
strip = BX - (px1-px0) - inset
print("Leerstreifen am pad-armen Ende: %.2f - %.4f - %.4f = %.4f mm" % (BX,px1-px0,inset,strip))
bx0 = px0 - strip; bx1 = px1 + inset
by0 = py0 - inset; by1 = py1 + inset
print()
print("Koerper daraus: x %.4f ... %.4f = %.4f  (Datenblatt %.1f)" % (bx0,bx1,bx1-bx0,BX))
print("                y %.4f ... %.4f = %.4f  (Datenblatt %.1f)" % (by0,by1,by1-by0,BY))
print()
print("Freihaltezone (Antennenende, %.1f breit x %.4f tief):" % (BY,strip))
print("   x %.4f ... %.4f    y %.4f ... %.4f" % (bx0,bx0+strip,by0,by1))
print()
print("alte, gerechnete Annahme: Mitte 17.75 + 15.5/2 = %.2f" % (17.75+BX/2))
print("Abstand Antennenkante zu den Platinenraendern:  x=0 -> %.2f mm   x=27 -> %.2f mm" % (bx0-0, 27-bx1))
