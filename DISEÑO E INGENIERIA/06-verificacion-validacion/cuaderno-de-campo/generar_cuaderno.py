# -*- coding: utf-8 -*-
"""Genera el cuaderno de campo imprimible para la validación del nodo (CCNN Paoyhan)."""
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
                                Table, TableStyle, Spacer, KeepTogether)

SALIDA = 'Cuaderno-de-campo-validacion-nodo.pdf'
GRIS   = colors.Color(.92,.92,.92)
LINEA  = colors.Color(.35,.35,.35)
TENUE  = colors.Color(.70,.70,.70)

P  = ParagraphStyle('p',  fontName='Helvetica', fontSize=8.2, leading=10.5)
PB = ParagraphStyle('pb', parent=P, fontName='Helvetica-Bold')
PS = ParagraphStyle('ps', parent=P, fontSize=7.2, leading=9, textColor=colors.Color(.3,.3,.3))
TH = ParagraphStyle('th', fontName='Helvetica-Bold', fontSize=6.6, leading=7.8, alignment=1)
H1 = ParagraphStyle('h1', fontName='Helvetica-Bold', fontSize=13, leading=15)
H2 = ParagraphStyle('h2', fontName='Helvetica-Bold', fontSize=9.5, leading=11,
                    textColor=colors.white, backColor=colors.Color(.25,.25,.25),
                    leftIndent=3, spaceBefore=0, spaceAfter=0)

CB = '[    ]'   # casilla para marcar

def barra(txt):
    t = Table([[Paragraph(txt, ParagraphStyle('b', fontName='Helvetica-Bold', fontSize=9,
                leading=11, textColor=colors.white))]], colWidths=[182*mm], rowHeights=[7*mm])
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),colors.Color(.25,.25,.25)),
                           ('LEFTPADDING',(0,0),(-1,-1),4),('VALIGN',(0,0),(-1,-1),'MIDDLE')]))
    return t

def campos(filas, anchos):
    """filas: lista de listas de (etiqueta, ancho_relleno). Dibuja etiqueta + línea para escribir."""
    datos, estilos = [], [('FONTNAME',(0,0),(-1,-1),'Helvetica'),('FONTSIZE',(0,0),(-1,-1),8),
                          ('VALIGN',(0,0),(-1,-1),'BOTTOM'),('TOPPADDING',(0,0),(-1,-1),3),
                          ('BOTTOMPADDING',(0,0),(-1,-1),4),('LEFTPADDING',(0,0),(-1,-1),2)]
    for r,fila in enumerate(filas):
        datos.append(fila)
        for c,celda in enumerate(fila):
            if celda == '':
                estilos.append(('LINEBELOW',(c,r),(c,r),0.5,LINEA))
    t = Table(datos, colWidths=anchos, rowHeights=[8.2*mm]*len(filas))
    t.setStyle(TableStyle(estilos))
    return t

def tabla(cabeceras, anchos, nfilas, alto=7.6*mm):
    datos = [[Paragraph(c, TH) for c in cabeceras]]
    datos += [['']*len(cabeceras) for _ in range(nfilas)]
    t = Table(datos, colWidths=anchos, rowHeights=[9*mm]+[alto]*nfilas, repeatRows=1)
    t.setStyle(TableStyle([
        ('GRID',(0,0),(-1,-1),0.5,LINEA),
        ('BACKGROUND',(0,0),(-1,0),GRIS),
        ('VALIGN',(0,0),(-1,0),'MIDDLE'),
        ('LEFTPADDING',(0,0),(-1,-1),2),('RIGHTPADDING',(0,0),(-1,-1),2),
    ]))
    return t

def renglones(n, ancho=182*mm, alto=7*mm):
    t = Table([['']]*n, colWidths=[ancho], rowHeights=[alto]*n)
    t.setStyle(TableStyle([('LINEBELOW',(0,0),(-1,-1),0.5,LINEA)]))
    return t

def cuadricula(ancho, alto, paso=6*mm):
    cols = int(ancho/paso); filas = int(alto/paso)
    t = Table([['']*cols]*filas, colWidths=[paso]*cols, rowHeights=[paso]*filas)
    t.setStyle(TableStyle([('GRID',(0,0),(-1,-1),0.25,TENUE),('BOX',(0,0),(-1,-1),0.6,LINEA)]))
    return t

class Doc(BaseDocTemplate):
    def __init__(self, fn, **kw):
        BaseDocTemplate.__init__(self, fn, pagesize=A4,
            leftMargin=14*mm, rightMargin=14*mm, topMargin=20*mm, bottomMargin=14*mm, **kw)
        marco = Frame(14*mm, 14*mm, 182*mm, 263*mm, id='n',
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([PageTemplate(id='base', frames=marco, onPage=self.encabezado)])
    def encabezado(self, c, d):
        c.saveState()
        c.setFont('Helvetica-Bold', 7.5)
        c.drawString(14*mm, 285*mm, 'PROYECTO AMAZONÍA+  ·  PILOTO OSINFOR  ·  CCNN PAOYHAN (LORETO)')
        c.setFont('Helvetica', 7.5)
        c.drawRightString(196*mm, 285*mm, 'Cuaderno de campo — validación del nodo')
        c.setLineWidth(0.7); c.line(14*mm, 283*mm, 196*mm, 283*mm)
        c.setFont('Helvetica', 6.8)
        c.setFillColor(colors.Color(.4,.4,.4))
        c.drawString(14*mm, 9*mm, 'Rellenar a lápiz.  No borrar: tachar con una línea, corregir al lado y poner iniciales.')
        c.drawRightString(196*mm, 9*mm, 'Página %d' % d.page)
        c.restoreState()

from reportlab.platypus import PageBreak
E = []
def sp(h=3): E.append(Spacer(1, h*mm))

# ═══════════════ 1 · portada
E.append(Paragraph('Cuaderno de campo', H1))
E.append(Paragraph('Validación del nodo de detección temprana de incendios (Edge AI + LoRaWAN)', P)); sp(4)
E.append(barra('DATOS DE LA CAMPAÑA')); sp(2)
E.append(campos([['Campaña N.º','','Del','','al','','Responsable','']],
                [24*mm,16*mm,10*mm,26*mm,8*mm,26*mm,24*mm,48*mm]))
E.append(campos([['Integrantes','','Nodos (ID)','','Gateway (ID)','']],
                [22*mm,58*mm,24*mm,34*mm,26*mm,18*mm]))
E.append(campos([['Versión de nodo  '+CB+' V1   '+CB+' V2   '+CB+' V3','','Firmware','',
                  'Modelo','']],
                [62*mm,8*mm,20*mm,32*mm,16*mm,44*mm])); sp(3)

E.append(barra('CÓMO LLENARLO')); sp(2)
for i in ['1.  <b>Coordenadas en grados decimales, WGS84, 5 decimales</b> (ej. −7.35812 / −74.90455). Hora local de 24 h.',
          '2.  Numerar las fotos de corrido durante toda la campaña y anotar el número. Sin ese número la foto no sirve de evidencia.',
          '3.  Si un dato no aplica, escribir <b>n/a</b>. A lápiz; para corregir, tachar con una línea y poner iniciales.',
          '4.  Los cuadros grises al pie de cada formato se llenan <b>al terminar</b>: ahí queda la conclusión.',
          '5.  Cada día se cierra firmando F-07.']:
    E.append(Paragraph(i, P)); E.append(Spacer(1, 1.8*mm))
sp(3)
E.append(barra('CONTENIDO')); sp(2)
cont = [[Paragraph(x, TH) for x in ['','Formato','Qué cierra','Requisito','Copias']]]
for f in [['F-01','Punto de instalación y puesta en marcha','Dónde y cómo quedó cada nodo','003 · 006','1 por nodo'],
          ['F-02','Cobertura LoRaWAN (walk test)','Alcance real del enlace','003','2'],
          ['F-03','Ensayo de detección de humo','Tasa de detección y latencia','001 · 008','3'],
          ['F-04','Alertas y falsos positivos','Comportamiento continuo','001','4'],
          ['F-05','Energía y ambiente','Autonomía solar','004 · 005','4'],
          ['F-06','Inspección física e IP67','Sellado y montaje','006 · 007','2 por nodo'],
          ['F-07','Cierre de jornada','Trazabilidad diaria','—','1 por día']]:
    cont.append([Paragraph('<b>'+f[0]+'</b>',P)]+[Paragraph(x,P) for x in f[1:]])
t = Table(cont, colWidths=[14*mm,62*mm,58*mm,26*mm,22*mm])
t.setStyle(TableStyle([('GRID',(0,0),(-1,-1),0.5,LINEA),('BACKGROUND',(0,0),(-1,0),GRIS),
                       ('VALIGN',(0,0),(-1,-1),'MIDDLE'),('TOPPADDING',(0,1),(-1,-1),2.5),
                       ('BOTTOMPADDING',(0,1),(-1,-1),2.5)]))
E.append(t); sp(2)
E.append(Paragraph('Los requisitos se citan abreviados: 001 = REQ-SYS-001, y así. '
                   'Ver <i>01-requisitos/requisitos-de-sistema.md</i>.', PS))
E.append(PageBreak())

# ═══════════════ F-01
E.append(Paragraph('F-01 · Punto de instalación y puesta en marcha', H1))
E.append(Paragraph('Una por nodo. Cierra REQ-SYS-003 y 006.', PS)); sp(3)
E.append(barra('IDENTIFICACIÓN Y UBICACIÓN')); sp(1.5)
E.append(campos([['Nodo ID','','DevEUI','','Fecha','','Hora','']],
                [17*mm,17*mm,18*mm,44*mm,13*mm,24*mm,12*mm,17*mm]))
E.append(campos([['Latitud','','Longitud','','Altitud (m)','','Precisión GPS (m)','']],
                [15*mm,31*mm,19*mm,31*mm,20*mm,16*mm,32*mm,10*mm]))
E.append(campos([['Nombre del lugar / sector','','Cómo se llega','']],
                [42*mm,40*mm,26*mm,74*mm])); sp(2)
E.append(barra('MONTAJE Y CAMPO DE VISIÓN')); sp(1.5)
E.append(campos([['Soporte  '+CB+' árbol  '+CB+' poste  '+CB+' otro:','','Altura (m)','',
                  'Azimut cámara (°)','','Inclinación (°)','']],
                [58*mm,16*mm,20*mm,12*mm,32*mm,12*mm,26*mm,6*mm]))
E.append(campos([['Dosel sobre el nodo  '+CB+' abierto  '+CB+' parcial  '+CB+' cerrado','',
                  'Obstrucciones en el campo de visión','']],
                [72*mm,6*mm,54*mm,50*mm])); sp(2)
E.append(barra('ENLACE Y PUESTA EN MARCHA')); sp(1.5)
E.append(campos([['Distancia al gateway (m)','','Línea de vista  '+CB+' sí  '+CB+' no','',
                  'Obstrucción principal','']],
                [42*mm,16*mm,42*mm,6*mm,38*mm,38*mm]))
E.append(campos([['Hora de encendido','','Join OTAA  '+CB+' OK  '+CB+' falló','','SF','',
                  'RSSI (dBm)','','SNR (dB)','']],
                [30*mm,14*mm,42*mm,6*mm,8*mm,12*mm,22*mm,14*mm,18*mm,16*mm]))
E.append(campos([['Batería (V)','','SoC (%)','','Primer heartbeat, hora','',
                  'Imagen de prueba  '+CB+' sí  '+CB+' no','']],
                [22*mm,14*mm,18*mm,12*mm,38*mm,14*mm,52*mm,12*mm])); sp(2)
E.append(barra('CROQUIS  (norte arriba · marcar nodo, campo de visión y gateway)')); sp(1.5)
E.append(cuadricula(182*mm, 84*mm)); sp(1.5)
E.append(campos([['Fotos N.º  del','','al','','Observaciones','','Iniciales','']],
                [26*mm,14*mm,8*mm,14*mm,28*mm,62*mm,20*mm,10*mm]))
E.append(PageBreak())

# ═══════════════ F-02 · cobertura
E.append(Paragraph('F-02 · Cobertura LoRaWAN (walk test)', H1))
E.append(Paragraph('Cierra REQ-SYS-003 (meta: 5 km). Alejarse del gateway tomando un punto cada '
                   '250–500 m. En cada punto, enviar <b>10 tramas de prueba</b> y anotar cuántas '
                   'llegaron. El porcentaje de pérdida se calcula después.', PS)); sp(3)
E.append(campos([['Fecha','','Gateway ID','','Nodo usado','','Modo  '+CB+' ADR  '+CB+' SF fijo:','']],
                [13*mm,20*mm,22*mm,20*mm,24*mm,20*mm,44*mm,19*mm])); sp(2)
E.append(tabla(['N.º','Hora','Latitud','Longitud','Distancia al<br/>gateway (m)','SF',
                'Llegaron<br/>de 10','RSSI<br/>(dBm)','SNR<br/>(dB)','Observaciones'],
               [10*mm,14*mm,24*mm,24*mm,22*mm,10*mm,16*mm,16*mm,14*mm,32*mm],
               15, alto=9*mm)); sp(3)
E.append(barra('AL TERMINAR')); sp(1.5)
E.append(campos([['Distancia máxima con 9 o 10 de 10','','m','',
                  '¿Se llegó a 5 km?  '+CB+' sí  '+CB+' no','','Iniciales','']],
                [52*mm,18*mm,6*mm,8*mm,48*mm,16*mm,20*mm,14*mm]))
E.append(campos([['Qué limitó el alcance','']],[36*mm,146*mm]))
E.append(renglones(1))
E.append(PageBreak())

# ═══════════════ F-03 · detección de humo
E.append(Paragraph('F-03 · Ensayo de detección de humo', H1))
E.append(Paragraph('Cierra REQ-SYS-001 (meta: 85 % de detección) y REQ-SYS-008 (latencia). '
                   'Generar humo controlado a distancias crecientes dentro del campo de visión.', PS)); sp(3)
E.append(campos([['Fecha','','Nodo ID','','Fuente de humo (material)','',
                  'Umbral de confianza (%)','']],
                [13*mm,20*mm,18*mm,18*mm,44*mm,30*mm,42*mm,12*mm]))
E.append(campos([['Cadencia de captura (min)','','Cielo','','Viento','','Temp. (°C)','']],
                [42*mm,12*mm,12*mm,32*mm,14*mm,32*mm,20*mm,12*mm])); sp(2)
E.append(tabla(['Ensayo<br/>N.º','Hora de<br/>inicio','Distancia<br/>al nodo (m)','Azimut<br/>(°)',
                '¿Detectó?<br/>S / N','Hora de<br/>la alerta','Confianza<br/>(%)','Foto<br/>N.º',
                'Observaciones'],
               [14*mm,18*mm,22*mm,14*mm,18*mm,20*mm,20*mm,12*mm,42*mm],
               11, alto=10.6*mm)); sp(3)
E.append(barra('AL TERMINAR')); sp(1.5)
E.append(campos([['Ensayos','','Detectados','','Tasa (%)','','Distancia máx. con detección (m)','',
                  'Iniciales','']],
                [16*mm,12*mm,22*mm,12*mm,16*mm,12*mm,54*mm,14*mm,18*mm,6*mm]))
E.append(campos([['¿Cumple el 85 %?  '+CB+' sí  '+CB+' no','','Notas sobre fallos','']],
                [42*mm,10*mm,32*mm,98*mm]))
E.append(renglones(1))
E.append(PageBreak())

# ═══════════════ F-04 · alertas
E.append(Paragraph('F-04 · Alertas y falsos positivos', H1))
E.append(Paragraph('Cierra REQ-SYS-001 en operación continua. Anotar <b>toda</b> alerta de humo, '
                   'verdadera o falsa. Los falsos positivos por neblina y nube baja son el riesgo '
                   'principal del modelo, y anotar su causa es lo que permite reentrenarlo.', PS)); sp(3)
E.append(campos([['Nodo(s) en operación','','Del','','al','']],
                [40*mm,52*mm,10*mm,32*mm,8*mm,40*mm])); sp(2)
E.append(tabla(['Fecha','Hora','Nodo','Confianza<br/>(%)','¿Humo real?<br/>S / N / dudoso',
                'Si es falso, causa probable<br/>(neblina · nube · sol · polvo · insecto · otro)',
                'Foto<br/>N.º','Observaciones'],
               [18*mm,14*mm,14*mm,18*mm,26*mm,54*mm,12*mm,26*mm],
               17, alto=9.4*mm)); sp(3)
E.append(barra('AL TERMINAR')); sp(1.5)
E.append(campos([['Alertas de humo','','Verdaderas','','Falsas','','Falsos positivos (%)','',
                  'Iniciales','']],
                [30*mm,12*mm,22*mm,12*mm,14*mm,12*mm,36*mm,12*mm,18*mm,14*mm]))
E.append(campos([['Causa de falso positivo más frecuente','']],[58*mm,124*mm]))
E.append(PageBreak())

# ═══════════════ F-05 · energía y ambiente
E.append(Paragraph('F-05 · Energía y ambiente', H1))
E.append(Paragraph('Cierra REQ-SYS-004 (autonomía de 2–3 días sin sol) y REQ-SYS-005 (clima '
                   'amazónico). Dos lecturas al día bastan: <b>temprano en la mañana</b>, antes de '
                   'que cargue el panel, y <b>al final de la tarde</b>. La temperatura se toma del '
                   'heartbeat, no hace falta termómetro aparte.', PS)); sp(3)
E.append(campos([['Nodo(s) / gateway','','Del','','al','']],
                [36*mm,56*mm,10*mm,32*mm,8*mm,40*mm])); sp(2)
E.append(tabla(['Fecha','Hora','Equipo<br/>nodo / GW','Batería<br/>(V)','SoC<br/>(%)',
                'Temp.<br/>(°C)','Cielo<br/>despejado / nubes / lluvia','¿Llovió<br/>anoche?',
                'Observaciones'],
               [18*mm,14*mm,22*mm,18*mm,14*mm,16*mm,40*mm,18*mm,22*mm],
               17, alto=9.4*mm)); sp(3)
E.append(barra('AL TERMINAR')); sp(1.5)
E.append(campos([['Días seguidos con poco sol','','¿Siguió operando?  '+CB+' sí  '+CB+' no','',
                  'Tensión mínima (V)','','Iniciales','']],
                [44*mm,12*mm,44*mm,10*mm,32*mm,12*mm,18*mm,10*mm]))
E.append(campos([['Notas (sombra sobre el panel, condensación, suciedad)','']],[74*mm,108*mm]))
E.append(PageBreak())

# ═══════════════ F-06 + F-07 en una sola hoja
E.append(Paragraph('F-06 · Inspección física e IP67', H1))
E.append(Paragraph('Cierra REQ-SYS-006 y 007. Al instalar, tras la primera lluvia fuerte y al retirar.', PS)); sp(2.5)
E.append(campos([['Fecha','','Nodo ID','','Momento  '+CB+' instalación  '+CB+' tras lluvia  '
                  +CB+' retiro','']],
                [13*mm,20*mm,18*mm,18*mm,76*mm,37*mm])); sp(1.5)
items = ['Gabinete cerrado, tornillos completos y empaque en su sitio',
         'Sin condensación ni agua dentro',
         'Prensaestopas apretados y cables con curva de goteo',
         'Lente y visor limpios, sin gotas ni telarañas',
         'Fijación firme al soporte y antena vertical',
         'Panel solar limpio, sin sombra y bien orientado',
         'Sin fauna, insectos ni nidos en el gabinete',
         'Sin oxidación ni señales de manipulación']
datos = [[Paragraph('Punto de inspección', TH), Paragraph('OK', TH),
          Paragraph('Observado', TH), Paragraph('Detalle / acción', TH)]]
for it in items:
    datos.append([Paragraph(it, P), CB, CB, ''])
t = Table(datos, colWidths=[84*mm,12*mm,18*mm,68*mm], rowHeights=[8*mm]+[7.4*mm]*len(items))
t.setStyle(TableStyle([('GRID',(0,0),(-1,-1),0.5,LINEA),('BACKGROUND',(0,0),(-1,0),GRIS),
                       ('VALIGN',(0,0),(-1,-1),'MIDDLE'),('ALIGN',(1,1),(2,-1),'CENTER'),
                       ('FONTNAME',(1,1),(2,-1),'Helvetica'),('FONTSIZE',(1,1),(2,-1),8),
                       ('LEFTPADDING',(0,0),(-1,-1),3)]))
E.append(t); sp(1.5)
E.append(campos([['Fotos N.º','','Iniciales','']],[20*mm,40*mm,20*mm,102*mm])); sp(4)

E.append(Paragraph('F-07 · Cierre de jornada', H1))
E.append(Paragraph('Una por día. Es lo que da trazabilidad a la campaña.', PS)); sp(2.5)
E.append(campos([['Fecha','','Jornada N.º','','Clima del día','','Participantes','']],
                [13*mm,20*mm,24*mm,12*mm,26*mm,32*mm,24*mm,31*mm])); sp(1.5)
E.append(barra('QUÉ SE HIZO')); sp(1.2); E.append(renglones(3)); sp(1.5)
E.append(barra('INCIDENCIAS Y DECISIONES TOMADAS EN CAMPO')); sp(1.2); E.append(renglones(3)); sp(1.5)
E.append(barra('PENDIENTE PARA MAÑANA  ·  LECCIÓN APRENDIDA')); sp(1.2); E.append(renglones(3)); sp(2)
E.append(campos([['Formatos llenados hoy  '+CB+' F-01  '+CB+' F-02  '+CB+' F-03  '+CB+' F-04  '
                  +CB+' F-05  '+CB+' F-06','','Fotos del día, del','','al','']],
                [104*mm,4*mm,34*mm,14*mm,8*mm,18*mm]))
E.append(campos([['Firma del responsable','','Segunda firma','']],[42*mm,50*mm,30*mm,60*mm]))

Doc(SALIDA).build(E)
print('generado:', SALIDA)
