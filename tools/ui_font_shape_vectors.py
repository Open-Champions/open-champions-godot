#!/usr/bin/env python3
"""Authored synthetic inputs; extract ONLY the two pinned research units to ignored work/.
Never loads disc data, catalogue, texture decoder or the inferred font renderer.
"""
import argparse, hashlib, json, pathlib, struct, subprocess
PIN = '576cbb1a969464175c80e1834b92e6d72b8beaae'
SHA = '12aab860371d01154776e1352b46634c41f9ef776158fed09a770c4c51d7e057'

def font(n=3, terminator=True, empty=False, tail=0):
    grec=0x214; kern=grec+n*16; tex=kern+(0 if empty else 24)+tail
    b=bytearray(tex+7)
    struct.pack_into('<H', b,0,n)
    struct.pack_into('<III',b,8,grec,kern,tex)
    for i in range(n):
        struct.pack_into('<8H',b,grec+i*16,65,0,65535,3,4,[32768,32767,65535][i%3],65535,0xffff)
    if not empty:
        struct.pack_into('<HHh',b,kern,65534,65535,-32768)
        struct.pack_into('<HHh',b,kern+6,0,0,32767)
        struct.pack_into('<HHh',b,kern+12,65535 if terminator else 1,2,-1)
        struct.pack_into('<HHh',b,kern+18,3,4,0)
    b[tex:]=bytes([0,1,128,255,17,33,77])
    return b.hex()

def inputs():
    fonts=[font(),font(0,empty=True),font(1,False),font(2,False,tail=5),font(0,True)]
    # No inferred minimum table offset or overlap rules; zero records and reversed
    # bounded kerning/texture offsets are valid empty tables in the reference.
    b=bytearray.fromhex(font(0,empty=True));struct.pack_into('<I',b,12,len(b));fonts.append(b.hex())
    shapes=[]
    for w,h in [(1,1),(2,3),(3,7),(4,8)]:
        rgba=bytes((i*37+11)%256 for i in range(w*h*4)).hex()
        for rows in sorted(set([0,1,h//2,h])):
            oh=2*rows if rows else h
            for crop in [None,[0,0,w,oh],[w-1,oh-1,1,1],[0,1,w,oh-1] if oh>1 else [0,0,1,1]]:
                shapes.append(dict(w=w,h=h,rows=rows,crop=crop,rgba=rgba))
    return dict(fonts=fonts,shapes=shapes)

def main():
    a=argparse.ArgumentParser();a.add_argument('--research',required=True);a.add_argument('--godot',required=True);a.add_argument('--scratch',default='work/ui-font-shape-reference');a.add_argument('--output',default='native/tests/ui_font_shape_vectors.txt');a=a.parse_args()
    repo=pathlib.Path(a.research).resolve()
    src=subprocess.check_output(['git','-C',str(repo),'show',PIN+':tools/godot/project/gameart/gameart.gd'])
    assert hashlib.sha256(src).hexdigest()==SHA
    s=src.decode();shape=s[s.index('static func _shape('):s.index('\n\n# ---- what the skins',s.index('static func _shape('))];fnt=s[s.index('class Fnt:'):s.index('\n\n# ---- one texture:',s.index('class Fnt:'))]
    work=pathlib.Path(a.scratch).resolve();work.mkdir(parents=True,exist_ok=True)
    (work/'project.godot').write_text('config_version=5\n[application]\nconfig/name="Synthetic font shape reference"\n')
    (work/'reference.gd').write_text('extends RefCounted\n'+shape+'\n'+fnt+'\n')
    (work/'inputs.json').write_text(json.dumps(inputs(),sort_keys=True))
    harness='''extends SceneTree
const Ref = preload("res://reference.gd")
func _initialize():
    var inputs = JSON.parse_string(FileAccess.get_file_as_string("res://inputs.json"))
    var lines = PackedStringArray()
    for hex in inputs.fonts:
        var d = Ref.Fnt.parse(String(hex).hex_decode())
        lines.append("F|" + hex + "|" + records(d.glyphs) + "|" + records(d.kerns) + "|" + d.texture.hex_encode())
    for c in inputs.shapes:
        var img = Ref._shape({"w":int(c.w),"h":int(c.h),"rgba":String(c.rgba).hex_decode()}, {"rows":int(c.rows),"rect":c.crop})
        var crop = "-" if c.crop == null else records([c.crop])
        lines.append("S|%d,%d,%d|%s|%s|%d,%d|%s" % [c.w,c.h,c.rows,crop,c.rgba,img.get_width(),img.get_height(),img.get_data().hex_encode()])
    var f = FileAccess.open("res://results.txt",FileAccess.WRITE)
    f.store_string("\\n".join(lines)+"\\n")
    print("REFERENCE_PASS ", lines.size())
    quit()
func records(rows):
    var result = PackedStringArray()
    for row in rows:
        var values = PackedStringArray()
        for v in row:
            values.append(str(int(v)))
        result.append(",".join(values))
    return ";".join(result)
'''
    (work/'harness.gd').write_text(harness)
    subprocess.run([str(pathlib.Path(a.godot).resolve()),'--headless','--path',str(work),'--script','res://harness.gd'],check=True,timeout=60)
    out=pathlib.Path(a.output);out.write_bytes((work/'results.txt').read_bytes())
    print('vectors_sha256='+hashlib.sha256(out.read_bytes()).hexdigest())
if __name__=='__main__':main()
