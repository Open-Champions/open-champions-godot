#!/usr/bin/env python3
"""Generate synthetic Unicode naming traces from externally supplied pinned research.
No executable or original localization table is opened. Output uses UTF-8 hex.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import types

PIN = '14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512'
SHA = '513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e'


def hx(text):
    value = text if isinstance(text, bytes) else text.encode('utf-8')
    return value.hex() or '-'


def generate(research, output):
    source = research / 'tools/ref_items.py'
    raw = source.read_bytes()
    committed = subprocess.run(['git', '-C', str(research), 'show', PIN + ':tools/ref_items.py'], check=True, capture_output=True).stdout
    if hashlib.sha256(raw).hexdigest() != SHA or raw != committed:
        raise ValueError('Expected pinned ref_items.py bytes from ' + PIN)
    sys.dont_write_bytecode = True
    module = types.ModuleType('external_ref_items')
    module.__file__ = str(source)
    exec(compile(raw, str(source), 'exec'), module.__dict__)
    r = module
    rows = []
    counts = {}
    def emit(op, args, call, seen):
        try:
            result = call()
            value = hx(result[0]) + ':' + str(result[1]) if isinstance(result, tuple) else hx(result)
        except (ValueError, TypeError, IndexError, KeyError) as error:
            value = '!' + type(error).__name__
        rows.append('\t'.join(('names', op, ';'.join(map(str,args)), value, ','.join(hx(key) for key in seen) or '-')))
        counts[op] = counts.get(op, 0) + 1
    texts = ['', 'a', 'ab', 'abc', '_m', '_f', '_n', '_p', '_q', 'x_ms', 'x_FS', 'x_mP', 'x_FP', 'xx_M', 'xx_f', 'x__', 'x_fz', 'xxms', 'é_m', '猫_fs', '😀_N', 'e\u0301_mp', 'a\x00_f', 'a_b_m', 'x|y', '100%_fs', '雪'*127, '雪'*128]
    languages = [-128,-1,0,1,2,3,4,5,127]
    for language in languages:
        for text in texts:
            emit('strip',[language,hx(text)],lambda t=text,l=language:r.strip_grammar_suffix(t,l),[])
    for text in texts + ['a'*126,'a'*127,'a'*128,'a'*129,'a'*10000,'é'*127,'😀'*127,'😀'*128,'x%y','%%']:
        emit('formatted',[hx(text)],lambda t=text:r._formatted(t),[])
    alternatives = ['', 'one', 'one|two', 'one_ms|two_fs|trois_mp|quatre_fp', '|', 'a|ab|abc|abcd', 'é_ms|猫_fs|😀_mp|e\u0301_fp', 'a||z|', 'a%|b', '界'*127, '界'*128]
    for language in languages:
        for text in alternatives:
            for form in [-2147483648,-6,-4,-3,-2,-1,0,1,2,3,4,6,2147483647]:
                seen=[]
                def lookup(key, text=text): seen.append(key);return text
                emit('affix',[language,form,hx('clé_猫'),hx(text)],lambda:r.affix_form(lookup,language,form,'clé_猫'),seen)
    def build(language,decorated,flags,prefix,suffix,noun,pre,suf,space=b' ',fmt=b'name_%s',name=b'blade+grade',missing=''):
        table=object.__new__(r.Tables)
        table.name_space=space;table.name_key_format=fmt;table.of_key=b'join_\xe9'
        p=object.__new__(r.Prefix);p.key=b'pre_\xf1';table.prefixes=[p]
        s=object.__new__(r.Suffix);s.key=b'suf_\xfc';table.suffixes=[s]
        d=object.__new__(r.Definition);d.name=name
        item=r.Item(d);item.flags=flags;item.prefix=prefix;item.suffix=suffix
        lookup_table={p.key.decode('latin-1'):pre,s.key.decode('latin-1'):suf,table.of_key.decode('latin-1'):'de'}
        try:
            lookup_table[(fmt%name.split(b'+')[0]).decode('latin-1')]=noun
            lookup_table[(fmt%name).decode('latin-1')]='FULL_'+noun
        except (ValueError,TypeError):
            pass
        if missing:lookup_table.pop(missing,None)
        seen=[]
        def lookup(key):seen.append(key);return lookup_table[key]
        lookup_pairs=','.join(hx(key)+'='+hx(value) for key,value in sorted(lookup_table.items()))
        args=[language,int(decorated),flags,prefix,suffix,hx(name),hx(fmt),hx(space),hx(table.of_key),hx(p.key),hx(s.key),lookup_pairs]
        emit('build',args,lambda:r.build_name(table,item,decorated,lookup,language),seen)
    nouns=['blade','blade_ms','blade_fs','blade_mp','blade_fp','blade_m','blade_f','blade_n','blade_p','blade_q','猫_FS','😀_m','ab','a','', '100%_fs', '雪'*128]
    for language in languages:
        for noun in nouns:
            for prefix,suffix in [(255,255),(0,255),(255,0),(0,0)]:
                for decorated,flags in [(False,0),(True,0),(True,0x200),(True,0x8000000000000000)]:
                    build(language,decorated,flags,prefix,suffix,noun,'vif_ms|vive_fs|vifs_mp|vives_fp','éclat_ms|éclat_fs|éclats_mp|éclats_fp')
        for pre,suf in [('x%y','tail'),('plain','x%y'),('雪'*128,'tail'),('plain','雪'*128),('a|b|c|d','tail'),('plain','a|b|c|d'),('one|two','tail'),('plain','')]:
            build(language,True,0,0,0,'épée_fs',pre,suf,space=b'\xa0',fmt=b'%%_%s_key',name=b'\xe9p\xe9e+magic')
        for prefix,suffix in [(2,0),(0,2),(2,2)]:
            build(language,True,0,prefix,suffix,'blade_ms','bright','tail')
            build(language,False,0,prefix,suffix,'blade_ms','bright','tail')
        build(language,True,0,0,0,'blade_ms','bright','tail',missing='pre_ñ')
        build(language,True,0,0,0,'blade_ms','bright','tail',missing='join_é')
        build(language,True,0,0,0,'blade_ms','bright','tail',fmt=b'%b',space=b'\x00')
    for fmt in [b'%8s',b'%.3s',b'%-8s',b'%010s',b'%+ #12.4s',b'%0s',b'%.s',b'%ls',b'%hb',b'%La',b'%r',b'%.9a',b'%%:%s:%%',b'%c',b'%3c',b'%.0c',b'%q',b'%',b'',b'%%',b'%s%s',b'%*s',b'%.*s',b'%d',b'%(key)s',b'%(',b'%10(key)s',b'%.2147483647s',b'%.2147483648s',b'%999999999999999999999999s',b'%5%',b'%*',b'%.*',b'%.',b'%l']:
        for name in [b'x',b"one'quote",b'both\'\"\\\n\x00\xff',b'abc+def']:
            for decorated in (False,True):
                build(0,decorated,0,255,255,'noun','pre','suf',fmt=fmt,name=name)
    if source.read_bytes()!=raw:raise ValueError('Reference changed during generation')
    content='# item_names_vectors_v1\n'+'\n'.join(rows)+'\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(content,encoding='ascii',newline='\n')
    actual=('\n'.join(rows)+'\n').encode('ascii')
    manifest={'schema_version':1,'reference_commit':PIN,'reference_path':'tools/ref_items.py','reference_sha256':SHA,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'vector_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'vector_count':len(rows),'op_counts':counts,'expected_trace_sha256':hashlib.sha256(actual).hexdigest(),'text_encoding':'localized Unicode codepoints serialized as UTF-8 hex; table keys Latin1 bytes','scope':'RTA naming reference subset; synthetic injected data only, no executable or original localized text, no Norrath or runtime proof','noun_key_formats':'single bytes argument; s/b/r/a/c conversions, width/precision/flags/length modifiers and percent literal; incompatible programs preserve ValueError/TypeError categories'}
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research-dir',required=True,type=Path)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'native/tests/vectors/item_names.tsv')
    args=parser.parse_args();generate(args.research_dir.resolve(),args.output.resolve())
