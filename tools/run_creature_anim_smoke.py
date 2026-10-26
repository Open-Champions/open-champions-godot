#!/usr/bin/env python3
"""Stage the new opt-in extension in an isolated authored-fixture project."""
import argparse,pathlib,shutil
from godot_matrix import run_logged
from godot_isolation import configure_user_dir
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--godot',required=True)
    parser.add_argument('--render',action='store_true')
    parser.add_argument('--dispatch',action='store_true')
    parser.add_argument('--freed',action='store_true',help='freed-object Callable refusals (review finding 8)')
    parser.add_argument('--project',type=pathlib.Path,default=ROOT/'work/godot-smoke')
    parser.add_argument('--user-dir-name',help='matrix-owned isolated user data name')
    args=parser.parse_args()
    project=args.project
    extension=project/'creature_anim'
    (extension/'bin').mkdir(parents=True,exist_ok=True)
    (project/'tests').mkdir(exist_ok=True)
    shutil.copy2(ROOT/'gdextension/creature_anim/creature_animation.gdextension',extension)
    shutil.copy2(ROOT/'godot/creature_anim/smoke.gd',extension)
    shutil.copy2(ROOT/'godot/creature_anim/dispatch_smoke.gd',extension)
    shutil.copy2(ROOT/'godot/creature_anim/freed_callable_smoke.gd',extension)
    libraries=list((ROOT/'godot/creature_anim/bin').glob('*template_debug.*'))
    if not libraries:raise SystemExit('Build gdextension/creature_anim first')
    for lib in libraries:shutil.copy2(lib,extension/'bin'/lib.name)
    for name in ('model_fixture.gd','animation_fixture.gd','skinning_fixture.gd'):
        shutil.copy2(ROOT/'godot/tests'/name,project/'tests'/name)
    (project/'project.godot').write_text('config_version=5\n[application]\nconfig/name="Creature Animation Contract Smoke"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    if args.user_dir_name:configure_user_dir(project,args.user_dir_name)
    # The documented startup duration avoids the 4.5.2 first-import shutdown bug.
    run_logged([args.godot,'--headless','--path',str(project),'--editor','--quit-after','120','--frame-delay','50'],project/'editor.log',timeout=120)
    script='freed_callable_smoke.gd' if args.freed else 'dispatch_smoke.gd' if args.dispatch else 'smoke.gd'
    cmd=[args.godot,'--path',str(project),'--script','res://creature_anim/'+script]
    if args.render:cmd+=['--','--render']
    else:cmd.insert(1,'--headless')
    run_logged(cmd,project/(script+'.log'),timeout=120)
if __name__=='__main__':main()
