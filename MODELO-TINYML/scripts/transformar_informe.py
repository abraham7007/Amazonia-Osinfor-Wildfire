"""Convierte el informe entregado por el proveedor en el informe de implementacion
del equipo UNI-OSINFOR.

Se opera sobre el XML del .docx en vez de rehacer el documento, para conservar
intactas las 50 fotografias, las tablas y el formato original.

Cambios:
  1. Logo de AIO Sensors -> logo de la UNI (cabecera). El de OSINFOR se conserva.
  2. Portada y cabecera: "Informe final de proyecto" -> "Informe de implementacion".
  3. Autoria: Abraham Caso Torres y Ayrthon Pucuhuayla Casas.
  4. Se retiran los anexos 4 y 5 (scripts del prototipo, basados en Edge Impulse).
  5. Numeracion de secciones coherente, y el anexo 6 pasa a ser el 4.
  6. Bloque de firmas al cierre.
  7. Se marca el indice para que Word lo regenere al abrir.
"""

import copy
import re
import shutil
import zipfile
from pathlib import Path

import xml.etree.ElementTree as ET
from PIL import Image

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
WNS = "{" + W + "}"
ET.register_namespace("w", W)
for pfx, uri in {
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    "wp": "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing",
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "pic": "http://schemas.openxmlformats.org/drawingml/2006/picture",
    "w14": "http://schemas.microsoft.com/office/word/2010/wordml",
    "mc": "http://schemas.openxmlformats.org/markup-compatibility/2006",
    "wps": "http://schemas.microsoft.com/office/word/2010/wordprocessingShape",
    "wpg": "http://schemas.microsoft.com/office/word/2010/wordprocessingGroup",
    "w15": "http://schemas.microsoft.com/office/word/2012/wordml",
    "v": "urn:schemas-microsoft-com:vml",
    "o": "urn:schemas-microsoft-com:office:office",
    "w10": "urn:schemas-microsoft-com:office:word",
    "m": "http://schemas.openxmlformats.org/officeDocument/2006/math",
    "wp14": "http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing",
    "wpi": "http://schemas.microsoft.com/office/word/2010/wordprocessingInk",
    "w16se": "http://schemas.microsoft.com/office/word/2015/wordml/symex",
    "w16cid": "http://schemas.microsoft.com/office/word/2016/wordml/cid",
    "w16": "http://schemas.microsoft.com/office/word/2018/wordml",
    "w16du": "http://schemas.microsoft.com/office/word/2023/wordml/word16du",
    "w16cex": "http://schemas.microsoft.com/office/word/2018/wordml/cex",
    "w16sdtdh": "http://schemas.microsoft.com/office/word/2020/wordml/sdtdatahash",
    "cx": "http://schemas.microsoft.com/office/drawing/2014/chartex",
}.items():
    ET.register_namespace(pfx, uri)

RAIZ = Path("/Users/mecatronica/Documents/edgeForest/AMAZONIA-OSINFOR")
FUENTE = RAIZ / "MODELO-TINYML" / "Informe Final Proyecto Osinfor 05 09.docx"
LOGO_UNI = RAIZ / "Uni-logo_transparente_granate (1).png"
DESTINO = RAIZ / "MODELO-TINYML" / "Informe de Implementacion - Nodos de Deteccion Temprana - OSINFOR UNI.docx"
TRABAJO = Path("docx")

TITULO_NUEVO = "INFORME DE IMPLEMENTACIÓN"
SUBTITULO_NUEVO = "EQUIPOS (HARDWARE), CONFIGURACIÓN Y PRUEBAS"
INSTITUCION = "UNIVERSIDAD NACIONAL DE INGENIERÍA"
AUTORES = ["Abraham Caso Torres", "Ayrthon Pucuhuayla Casas"]


# --------------------------------------------------------------------------
# 1. Logo
# --------------------------------------------------------------------------
def poner_logo_uni():
    """Sustituye image43.png (AIO Sensors) por el logo de la UNI.

    El logo de la UNI es vertical (0,795) y el de AIO horizontal (1,769): si se
    intercambian los ficheros sin mas, Word estira la imagen sobre el marco viejo
    y el escudo sale deformado. Por eso se reescribe tambien el `extent` de la
    cabecera con la proporcion correcta, tomando como altura la del logo de
    OSINFOR para que los dos queden visualmente equilibrados.
    """
    logo = Image.open(LOGO_UNI).convert("RGBA")
    aspecto = logo.width / logo.height
    alto_px = 620
    logo = logo.resize((round(alto_px * aspecto), alto_px), Image.LANCZOS)
    logo.save(TRABAJO / "word" / "media" / "image43.png", "PNG")

    cab = (TRABAJO / "word" / "header1.xml").read_text(encoding="utf-8")

    cy_osinfor = 615315           # altura del logo de OSINFOR, en EMU
    cy = cy_osinfor
    cx = round(cy * aspecto)
    # `a:ext` lleva en el original un 3,125 % de margen sobre `wp:extent`.
    margen = 809184 / 784670
    cab = cab.replace('<wp:extent cx="784670" cy="443601"/>',
                      f'<wp:extent cx="{cx}" cy="{cy}"/>', 1)
    cab = cab.replace('<a:ext cx="809184" cy="457460"/>',
                      f'<a:ext cx="{round(cx*margen)}" cy="{round(cy*margen)}"/>', 1)
    # El titulo esta partido en dos runs ("INFORME FINAL" + " DE PROYECTO"),
    # asi que no basta con reemplazar la cadena completa.
    cab = cab.replace("<w:t>INFORME FINAL</w:t>", f"<w:t>{TITULO_NUEVO}</w:t>", 1)
    cab, n = re.subn(r"<w:t[^>]*> DE PROYECTO</w:t>",
                     '<w:t xml:space="preserve"></w:t>', cab, count=1)
    if n != 1 or "INFORME FINAL" in cab or "DE PROYECTO" in cab:
        raise SystemExit("el titulo de la cabecera no se pudo reemplazar por completo")
    # Texto alternativo de la imagen: seguia nombrando al proveedor anterior.
    cab = cab.replace('descr="INDUSTRIAS - AIO Sensors"',
                      'descr="Universidad Nacional de Ingenieria"')
    cab = cab.replace('name="Imagen 2"', 'name="Logo UNI"')
    (TRABAJO / "word" / "header1.xml").write_text(cab, encoding="utf-8")
    return aspecto, cx, cy


# --------------------------------------------------------------------------
# 2. Utilidades sobre parrafos
# --------------------------------------------------------------------------
def texto(p):
    return "".join(n.text or "" for n in p.iter(WNS + "t")).strip()


def estilo(p):
    s = p.find(WNS + "pPr/" + WNS + "pStyle")
    return s.get(WNS + "val") if s is not None else ""


def poner_texto(p, lineas):
    """Reescribe el texto de un parrafo conservando el formato de su primer run."""
    runs = p.findall(WNS + "r")
    if not runs:
        return False
    modelo = runs[0]
    rpr = modelo.find(WNS + "rPr")
    for r in runs[1:]:
        p.remove(r)
    for hijo in list(modelo):
        if hijo.tag != WNS + "rPr":
            modelo.remove(hijo)
    for i, linea in enumerate(lineas):
        if i:
            ET.SubElement(modelo, WNS + "br")
        t = ET.SubElement(modelo, WNS + "t")
        t.text = linea
        t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    return True


def parrafo_como(modelo, lineas, estilo_nombre=None, centrado=True):
    """Crea un parrafo nuevo copiando el formato de `modelo`."""
    p = copy.deepcopy(modelo)
    for hijo in list(p):
        if hijo.tag not in (WNS + "pPr",):
            p.remove(hijo)
    ppr = p.find(WNS + "pPr")
    if ppr is None:
        ppr = ET.SubElement(p, WNS + "pPr")
        p.remove(ppr)
        p.insert(0, ppr)
    for s in ppr.findall(WNS + "pStyle"):
        ppr.remove(s)
    if estilo_nombre:
        s = ET.Element(WNS + "pStyle")
        s.set(WNS + "val", estilo_nombre)
        ppr.insert(0, s)
    for j in ppr.findall(WNS + "jc"):
        ppr.remove(j)
    if centrado:
        j = ET.SubElement(ppr, WNS + "jc")
        j.set(WNS + "val", "center")

    r = ET.SubElement(p, WNS + "r")
    rpr = ET.SubElement(r, WNS + "rPr")
    f = ET.SubElement(rpr, WNS + "rFonts")
    f.set(WNS + "ascii", "Arial")
    f.set(WNS + "hAnsi", "Arial")
    f.set(WNS + "cs", "Arial")
    sz = ET.SubElement(rpr, WNS + "sz")
    sz.set(WNS + "val", "24")
    szcs = ET.SubElement(rpr, WNS + "szCs")
    szcs.set(WNS + "val", "24")
    for i, linea in enumerate(lineas):
        if i:
            ET.SubElement(r, WNS + "br")
        t = ET.SubElement(r, WNS + "t")
        t.text = linea
        t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    return p


# Indice: titulo y pagina de cada entrada que sobrevive. Las paginas se
# comprobaron sobre el PDF generado tras quitar los dos anexos de codigo. Word
# las recalcula igualmente al abrir (settings.xml pide actualizar campos), pero
# asi el documento tambien es correcto si nadie regenera nada.
ENTRADAS_INDICE = [
    ("1.", "Objetivo", 3),
    ("2.", "Desarrollo del prototipo", 3),
    ("3.", "Pruebas de funcionamiento", 5),
    ("4.", "Armado de módulos de carga para baterías y panel solar", 8),
    ("5.", "Ensamblaje final", 10),
    ("6.", "Armado del tablero y gateway", 12),
    ("7.", "Conclusiones", 14),
    ("8.", "Anexos", 15),
    ("", "Anexo N.°1 : Configuración Gateway UG67", 15),
    ("", "Anexo N.°2: Configuración Node-RED", 17),
    ("", "Anexo N.°3 : Armado del tablero y montaje del Gateway", 18),
    ("", "Anexo N.°4. Materiales extras utilizados:", 24),
]


def rehacer_indice(body):
    """Reescribe el indice guardado.

    Hace falta porque el indice es texto congelado: si no se toca, sigue
    anunciando "Anexo N.°4 : Script CamThink" y "Anexo N.°5 : Script Xiao Seeed",
    que es justo lo que se ha retirado, y con la numeracion vieja rota.
    """
    sdt = next(e for e in body if e.tag == WNS + "sdt")
    contenido = sdt.find(WNS + "sdtContent")
    entradas = [p for p in contenido
                if p.tag == WNS + "p" and p.find(WNS + "hyperlink") is not None]

    # Las dos entradas de los scripts desaparecen.
    for p in entradas:
        texto_entrada = "".join(n.text or "" for n in p.iter(WNS + "t"))
        if "Script CamThink" in texto_entrada or "Script Xiao Seeed" in texto_entrada:
            contenido.remove(p)
    entradas = [p for p in contenido
                if p.tag == WNS + "p" and p.find(WNS + "hyperlink") is not None]

    if len(entradas) != len(ENTRADAS_INDICE):
        raise SystemExit(
            f"el indice tiene {len(entradas)} entradas y se esperaban {len(ENTRADAS_INDICE)}")

    for p, (numero, titulo, pagina) in zip(entradas, ENTRADAS_INDICE):
        enlace = p.find(WNS + "hyperlink")
        celdas = enlace.findall(".//" + WNS + "t")
        if len(celdas) < 2:
            continue
        # La ultima celda de texto es siempre el numero de pagina; las de delante,
        # el titulo. Unas entradas venian partidas en dos runs (numero + texto,
        # con un tabulador en medio) y otras en uno solo, lo que las dejaba con
        # sangrias distintas. Se unifican todas al mismo formato: titulo completo
        # en el primer run y sin tabulador de separacion.
        celdas[0].text = (numero + " " + titulo).strip()
        for c in celdas[1:-1]:
            c.text = ""
        quitar_tabulador_de_sangria(enlace)
        celdas[-1].text = str(pagina)
        for c in celdas:
            c.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")

    return len(entradas)


def quitar_tabulador_de_sangria(enlace):
    """Elimina el tabulador que separaba el numero del titulo en una entrada.

    Solo el primero: el segundo tabulador es el que lleva la linea de puntos
    hasta el numero de pagina y debe quedarse.
    """
    for r in enlace.findall(WNS + "r"):
        if r.find(WNS + "tab") is not None and r.find(WNS + "rPr/" + WNS + "webHidden") is None:
            enlace.remove(r)
            return True
    return False


def suprimir_numeracion_automatica(p):
    """Cancela la numeracion que el parrafo hereda del estilo Titulo 1.

    El original numeraba dos veces: el estilo ponia su contador y ademas algunos
    titulos traian el numero escrito a mano. Peor aun, el contador se reiniciaba
    a mitad del documento, con lo que el indice quedaba 1, 2, 3, 2, 3, 4, 5, 6.
    Declarar numId=0 en el parrafo anula la numeracion heredada; el numero pasa a
    ser el del texto, que se controla desde un solo sitio.
    """
    ppr = p.find(WNS + "pPr")
    if ppr is None:
        ppr = ET.Element(WNS + "pPr")
        p.insert(0, ppr)
    for viejo in ppr.findall(WNS + "numPr"):
        ppr.remove(viejo)
    numpr = ET.Element(WNS + "numPr")
    ilvl = ET.SubElement(numpr, WNS + "ilvl")
    ilvl.set(WNS + "val", "0")
    numid = ET.SubElement(numpr, WNS + "numId")
    numid.set(WNS + "val", "0")
    # numPr va detras de pStyle y antes del resto, segun el esquema.
    pos = 1 if ppr.find(WNS + "pStyle") is not None else 0
    ppr.insert(pos, numpr)


def tabla_firmas(modelo_p):
    """Tabla de dos columnas sin bordes con la linea de firma de cada autor."""
    tbl = ET.Element(WNS + "tbl")
    pr = ET.SubElement(tbl, WNS + "tblPr")
    est = ET.SubElement(pr, WNS + "tblW")
    est.set(WNS + "w", "5000")
    est.set(WNS + "type", "pct")
    bordes = ET.SubElement(pr, WNS + "tblBorders")
    for lado in ("top", "left", "bottom", "right", "insideH", "insideV"):
        b = ET.SubElement(bordes, WNS + lado)
        b.set(WNS + "val", "none")
        b.set(WNS + "sz", "0")
        b.set(WNS + "space", "0")
    grid = ET.SubElement(tbl, WNS + "tblGrid")
    for _ in AUTORES:
        gc = ET.SubElement(grid, WNS + "gridCol")
        gc.set(WNS + "w", "4839")
    tr = ET.SubElement(tbl, WNS + "tr")
    for nombre in AUTORES:
        tc = ET.SubElement(tr, WNS + "tc")
        tcpr = ET.SubElement(tc, WNS + "tcPr")
        w = ET.SubElement(tcpr, WNS + "tcW")
        w.set(WNS + "w", "4839")
        w.set(WNS + "type", "dxa")
        tc.append(parrafo_como(modelo_p, [""]))
        tc.append(parrafo_como(modelo_p, ["_______________________________"]))
        tc.append(parrafo_como(modelo_p, [nombre]))
        tc.append(parrafo_como(modelo_p, ["Universidad Nacional de Ingeniería"]))
    return tbl


# --------------------------------------------------------------------------
# 3. Documento
# --------------------------------------------------------------------------
def transformar_documento():
    ruta = TRABAJO / "word" / "document.xml"
    arbol = ET.parse(ruta)
    body = arbol.getroot().find(WNS + "body")
    hijos = list(body)
    informe = []

    # --- portada ---
    poner_texto(hijos[2], [TITULO_NUEVO]);            informe.append("portada: titulo")
    poner_texto(hijos[8], [SUBTITULO_NUEVO]);         informe.append("portada: subtitulo")
    poner_texto(hijos[15], ["Elaborado por:"])
    poner_texto(hijos[17], [INSTITUCION]);            informe.append("portada: institucion -> UNI")
    poner_texto(hijos[19], ["Autores: " + AUTORES[0], AUTORES[1]])
    informe.append("portada: autores -> " + " y ".join(AUTORES))

    # --- numeracion de secciones ---
    # El original mezclaba titulos numerados y sin numerar ("Objetivo",
    # "2. Desarrollo...", "Armado de modulos...").
    nuevos_titulos = [
        "1. Objetivo",
        "2. Desarrollo del prototipo",
        "3. Pruebas de funcionamiento",
        "4. Armado de módulos de carga para baterías y panel solar",
        "5. Ensamblaje final",
        "6. Armado del tablero y gateway",
        "7. Conclusiones",
        "8. Anexos",
    ]
    idx_t1 = [i for i, e in enumerate(hijos) if estilo(e) == "Ttulo1"]
    assert len(idx_t1) == len(nuevos_titulos), (len(idx_t1), len(nuevos_titulos))
    for i, titulo in zip(idx_t1, nuevos_titulos):
        suprimir_numeracion_automatica(hijos[i])
        poner_texto(hijos[i], [titulo])
    informe.append(f"secciones renumeradas 1-8 ({len(idx_t1)} titulos)")

    idx_anexos = idx_t1[-1]

    # --- retirar los anexos de codigo ---
    i4 = next(i for i, e in enumerate(hijos) if texto(e).startswith("Anexo N.°4"))
    i6 = next(i for i, e in enumerate(hijos) if texto(e).startswith("Anexo N.°6"))
    for e in hijos[i4:i6]:
        body.remove(e)
    informe.append(f"retirados los anexos 4 y 5 (scripts del prototipo): {i6 - i4} elementos")

    poner_texto(hijos[i6], ["Anexo N.°4. Materiales extras utilizados:"])
    informe.append("anexo 6 renumerado como anexo 4")

    # --- nota que sustituye a los scripts retirados ---
    nota = parrafo_como(
        hijos[idx_anexos + 1],
        ["Nota. Los scripts de firmware del prototipo, basados en un modelo "
         "preliminar entrenado en Edge Impulse (F1 = 0,52), se retiran de este "
         "informe por haber quedado superados. El firmware de los nodos, las "
         "pruebas de hardware y el modelo TinyML entrenado para el piloto se "
         "mantienen versionados en el repositorio del proyecto."],
        centrado=False,
    )
    body.insert(list(body).index(hijos[idx_anexos]) + 1, nota)
    informe.append("nota anadida bajo la seccion de anexos")

    # --- bloque de firmas, antes de los anexos ---
    modelo = hijos[idx_anexos + 2]
    pos = list(body).index(hijos[idx_anexos])
    bloque = [
        parrafo_como(modelo, [""]),
        parrafo_como(modelo, [""]),
        tabla_firmas(modelo),
        parrafo_como(modelo, [""]),
        parrafo_como(modelo, ["Lima, septiembre de 2026"]),
        parrafo_como(modelo, [""]),
    ]
    for k, e in enumerate(bloque):
        body.insert(pos + k, e)
    informe.append("bloque de firmas insertado antes de los anexos")

    n = rehacer_indice(body)
    informe.append(f"indice rehecho: {n} entradas, sin los anexos de codigo")

    arbol.write(ruta, encoding="UTF-8", xml_declaration=True, default_namespace=None)
    # ET escribe standalone="no"; Word lo tolera, pero se normaliza igualmente.
    crudo = ruta.read_text(encoding="utf-8")
    if "standalone" not in crudo.split("?>")[0]:
        crudo = crudo.replace("<?xml version='1.0' encoding='UTF-8'?>",
                              '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 1)
        ruta.write_text(crudo, encoding="utf-8")
    return informe


def marcar_indice_para_actualizar():
    """El indice es un campo TOC: se le pide a Word que lo regenere al abrir,
    porque tras quitar dos anexos y renumerar todo, el texto guardado ya no vale."""
    ruta = TRABAJO / "word" / "settings.xml"
    s = ruta.read_text(encoding="utf-8")
    if "updateFields" in s:
        return "el indice ya estaba marcado para actualizarse"
    s = s.replace("<w:settings ", "<w:settings ", 1)
    # Se inserta justo antes del cierre, que es posicion valida en el esquema.
    s = s.replace("</w:settings>", '<w:updateFields w:val="true"/></w:settings>', 1)
    ruta.write_text(s, encoding="utf-8")
    return "indice marcado para regenerarse al abrir en Word"


def reempaquetar():
    original = zipfile.ZipFile(FUENTE)
    orden = original.namelist()
    original.close()
    if DESTINO.exists():
        DESTINO.unlink()
    with zipfile.ZipFile(DESTINO, "w", zipfile.ZIP_DEFLATED) as z:
        for nombre in orden:
            z.write(TRABAJO / nombre, nombre)


if __name__ == "__main__":
    aspecto, cx, cy = poner_logo_uni()
    print(f"logo UNI: aspecto {aspecto:.3f}, marco {cx}x{cy} EMU "
          f"({cx/914400*2.54:.2f} x {cy/914400*2.54:.2f} cm)")
    for linea in transformar_documento():
        print(" -", linea)
    print(" -", marcar_indice_para_actualizar())
    reempaquetar()
    print(f"\nescrito: {DESTINO}")
    print(f"tamano:  {DESTINO.stat().st_size/1024/1024:.2f} MB")
