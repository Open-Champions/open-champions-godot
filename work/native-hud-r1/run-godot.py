import json,pathlib,subprocess,time
root=pathlib.Path.cwd(); out=root/'work/native-hud-r1'
godot=str(root.parent/'integration/work/runtime/Godot.app/Contents/MacOS/Godot')
results=[]
commands=[('import',[godot,'--headless','--path','godot','--editor','--quit-after','120','--frame-delay','50'])]
for name in ['archive','frame_rate','session_facing','session_recovery','session_sandbox','texture_upscale','ui_art_gallery','control_anim_cache','level_library','scene_animation_preview','scene_original_art','scene_play_flow','playable_wave']:
    commands.append((name,[godot,'--headless','--path','godot','--script','res://tests/'+name+'_smoke.gd']))
commands.append(('play_session_runtime',[godot,'--headless','--path','godot','--script','res://tests/play_session_runtime.gd']))
commands.append(('combined-play-short',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_play.gd','--','--ready-ticks=640']))
for name in ['inventory_icons_smoke','ui_layout_smoke','ui_layout_dynamic_smoke']:
    commands.append((name,[godot,'--headless','--path','godot','--script','res://tests/'+name+'.gd']))
commands.append(('combined-ui',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_ui.gd','--','--ready-ticks=640']))
commands.append(('combined-level',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_level.gd','--','--ready-ticks=640']))
commands.append(('combined-animation',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_animation.gd','--','--ready-ticks=640']))
commands.append(('native-fx',[godot,'--headless','--path','godot','--script','res://tests/native_fx_smoke.gd']))
commands.append(('combined-fx',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_fx.gd','--','--ready-ticks=640']))
commands.append(('default',[godot,'--headless','--path','godot','--quit-after','120']))
for name in ['level_texture_preview_smoke','level_textured_session_smoke']:
    commands.append((name,[godot,'--headless','--path','godot','--script','res://tests/'+name+'.gd']))
commands.append(('combined-textures',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_textures.gd','--','--ready-ticks=640']))
commands.append(('level_texture_material',[godot,'--path','godot','--script','res://tests/level_texture_material_smoke.gd']))
commands.append(('native-audio',[godot,'--headless','--path','godot','--script','res://tests/native_audio_smoke.gd']))
commands.append(('combined-full-combat',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_full_combat.gd','--','--ready-ticks=640']))
commands.append(('fx-camera-headless',[godot,'--headless','--path','godot','--script','res://tests/playable_fx_camera_smoke.gd']))
commands.append(('fx-camera-rendered',[godot,'--path','godot','--script','res://tests/playable_fx_camera_smoke.gd']))
commands.append(('native-fx-visual',[godot,'--path','godot','--rendering-method','gl_compatibility','--script','res://tests/native_fx_visual.gd']))
commands.append(('fx-bridge-refusals',[godot,'--headless','--path','godot','--script','res://tests/fx_bridge_refusal_smoke.gd']))
commands.append(('level-lights',[godot,'--headless','--path','godot','--script','res://tests/level_lights_smoke.gd']))
commands.append(('combined-combat',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_combat.gd','--','--ready-ticks=640']))
commands.append(('wave12-combat-extensions',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_wave12.gd']))
commands.append(('real-data-empty',[godot,'--headless','--path','godot','res://scenes/real_data_play.tscn','--quit-after','120']))
commands.append(('real-data-empty-script',[godot,'--headless','--path','godot','res://scenes/real_data_play.tscn','--quit-after','120','--','--level-script']))
commands.append(('script-commands',[godot,'--headless','--path','godot','--script','res://tests/script_commands_smoke.gd']))
commands.append(('session-music',[godot,'--headless','--path','godot','--script','res://tests/session_music_smoke.gd']))
commands.append(('creature-anim',['python3','tools/run_creature_anim_smoke.py','--godot',godot]))
commands.append(('creature-anim-freed',['python3','tools/run_creature_anim_smoke.py','--godot',godot,'--freed']))
commands.append(('creature-anim-dispatch',['python3','tools/run_creature_anim_smoke.py','--godot',godot,'--dispatch']))
for name in ['level_texture_stream_smoke','fx_shade_batch_smoke','ps2_save_import_smoke']:
    commands.append((name,[godot,'--headless','--path','godot','--script','res://tests/'+name+'.gd']))
commands.append(('creature-blend-gl',[godot,'--path','godot','--rendering-method','gl_compatibility','--script','res://tests/creature_blend_smoke.gd']))
commands.append(('creature-blend-forward',[godot,'--path','godot','--script','res://tests/creature_blend_smoke.gd']))
commands.append(('combined-audio',[godot,'--headless','--path','godot','--script','res://tests/playable_wave_smoke_audio.gd','--','--ready-ticks=640']))
commands.append(('play-hud',[godot,'--headless','--path','godot','--script','res://tests/play_hud_smoke.gd']))
commands.append(('menu',[godot,'--headless','--path','godot','--script','res://tests/menu_smoke.gd']))
commands.append(('frontend',[godot,'--headless','--path','godot','--script','res://tests/frontend_smoke.gd']))
for name,cmd in commands:
    start=time.monotonic()
    with (out/('godot-'+name+'.log')).open('w') as log:
        try: code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=1200).returncode
        except subprocess.TimeoutExpired: code=124
    result=dict(name=name,command=cmd,exit=code,seconds=round(time.monotonic()-start,2))
    results.append(result)
    (out/'godot-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(result),flush=True)
